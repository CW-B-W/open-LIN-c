/*
 * open_lin_master_data_layer.c
 *
 *  Created on: 30.01.2018
 *      Author: ay7vi2
 */

#include "open_lin_types.h"
#include "open_lin_data_layer.h"
#include "open_lin_network_layer.h"
#include "open_lin_hw.h"
#include "open_lin_master_data_layer.h"

static t_open_lin_master_state lin_master_state;
static void (*lin_master_state_callback)(t_open_lin_master_state);

static l_u8 master_rx_count = 0;
static l_u8 master_table_index = 0;
static t_master_frame_table_item *master_frame_table;
static l_u8 master_frame_table_size = 0;

static l_u32 frame_start_time_us = 0;

static void open_lin_master_goto_idle(l_bool next_item);
static void data_layer_next_item(void);
static t_master_frame_table_item* get_current_item(void);

static void open_lin_master_set_state(t_open_lin_master_state new_state)
{
	lin_master_state = new_state;
	if (lin_master_state_callback) {
		lin_master_state_callback(lin_master_state);
	}
}

static void open_lin_master_goto_idle(l_bool next_item)
{
	master_rx_count = 0;
	open_lin_master_set_state(OPEN_LIN_MASTER_IDLE);
	if (next_item)
	{
		data_layer_next_item();
	} else {
		/* do nothing */
	}

	open_lin_set_rx_enabled(l_false); // Prevent receiving data transmitted by itself(Master)
}


void open_lin_master_dl_init(t_master_frame_table_item *p_master_frame_table, l_u8 p_master_frame_table_size)
{
	master_frame_table = p_master_frame_table;
	master_frame_table_size = p_master_frame_table_size;
	open_lin_master_goto_idle(l_false);
}

static t_master_frame_table_item* get_current_item(void)
{
	return &(master_frame_table[master_table_index]);
}

static void data_layer_next_item(void)
{

	if (master_table_index >= (l_u8)(master_frame_table_size - 1u))
	{
		master_table_index = 0;
	} else {
		master_table_index ++;
	}
}


l_bool open_lin_master_data_tx_header(open_lin_frame_slot_t  *slot)
{
	l_bool result = l_true;
	result = (open_lin_hw_tx_break()) && result;
	result = (open_lin_hw_tx_byte(0x55)) && result;
	result = (open_lin_hw_tx_byte(open_lin_data_layer_parity(slot->pid))) && result;
	return result;
}

l_bool open_lin_master_data_tx_data(open_lin_frame_slot_t *slot)
{
	l_u8 i = 0;
	l_bool result = l_true;
	for (i = 0; i < slot->data_length; i++)
	{
		result = (open_lin_hw_tx_byte(slot->data_ptr[i])) && result;
	}

	result = (open_lin_hw_tx_byte(open_lin_data_layer_checksum(slot->pid, slot->data_length, slot->data_ptr))) && result;
	return result;
}

l_bool open_lin_master_dl_rx(l_u8 rx_byte)
{
	open_lin_frame_slot_t* slot = &(get_current_item()->slot);
	static l_u8 frame_tmp_buffer[OPEN_LIN_MAX_FRAME_LENGTH];
	l_bool pending = false;

	if (lin_master_state == OPEN_LIN_MASTER_DATA_RX)
	{
		if (master_rx_count < slot->data_length)
		{
			frame_tmp_buffer[master_rx_count] = rx_byte;
			master_rx_count ++;
			pending = l_true;
		} else
		{
			if (rx_byte == open_lin_data_layer_checksum(slot->pid,slot->data_length,frame_tmp_buffer))
			{
				(void)open_lin_memcpy(slot->data_ptr,frame_tmp_buffer,slot->data_length);
				open_lin_master_dl_rx_callback(slot);
			} else
			{
				open_lin_error_handler(OPEN_LIN_MASTER_ERROR_CHECKSUM);
			}
			open_lin_master_goto_idle(l_true);
		}
	}
	return pending;
}


void open_lin_master_dl_handler()
{
	t_master_frame_table_item* master_table_item = get_current_item();

	if (master_frame_table_size > 0u)
	{
		if (lin_master_state == OPEN_LIN_MASTER_IDLE)
		{
			frame_start_time_us = open_lin_hw_get_time_us();

			if (open_lin_master_data_tx_header(&master_table_item->slot) == l_true)
			{
				if (master_table_item->slot.frame_type == OPEN_LIN_FRAME_TYPE_TRANSMIT)
				{
					open_lin_master_set_state(OPEN_LIN_MASTER_TX_DATA);
				} else
				{
					open_lin_master_set_state(OPEN_LIN_MASTER_DATA_RX);
					master_rx_count = 0;
					open_lin_set_rx_enabled(l_true); // Ready for receiving from Slave
				}
			} else
			{
				open_lin_error_handler(OPEN_LIN_MASTER_ERROR_HEADER_TX);
				open_lin_master_set_state(OPEN_LIN_MASTER_IDLE);
			}
		} else
		{
			l_u32 frame_time_passed = open_lin_hw_get_time_us() - frame_start_time_us;
			if (frame_time_passed > master_table_item->frame_slot_us)
			{
				open_lin_error_handler(OPEN_LIN_MASTER_ERROR_FRAME_SLOT_TIMEOUT);
				open_lin_master_goto_idle(l_true);
				return;
			}
		}
		
		switch (lin_master_state)
		{
			case OPEN_LIN_MASTER_IDLE:
			{
				/* do nothing */
				break;
			}
			case OPEN_LIN_MASTER_DATA_RX:
			{
				l_u8 i = 0;
				l_u8 len = 1 + master_table_item->slot.data_length; // extra 1 bytes for checksum
				for (i = 0; i < len; i++) {
					const l_u32 read_timeout = 1000;
					l_bool frame_timeout = false;
					l_u8 bytes_read = 0;
					do {
						bytes_read = open_lin_hw_rx_byte(&master_table_item->slot.data_ptr[i], read_timeout);
						frame_timeout = (open_lin_hw_get_time_us() - frame_start_time_us) > master_frame_table->frame_slot_us;
					} while (bytes_read <= 0 && !frame_timeout);
					if (frame_timeout) {
						open_lin_error_handler(OPEN_LIN_MASTER_ERROR_FRAME_SLOT_TIMEOUT);
						open_lin_master_goto_idle(l_true);
						return;
					}
					else {
						open_lin_master_dl_rx(master_table_item->slot.data_ptr[i]);
					}
				}
				break;
			}

			case OPEN_LIN_MASTER_TX_DATA:
			{
				if (open_lin_master_data_tx_data(&master_table_item->slot) == l_true)
				{
					open_lin_master_goto_idle(l_true);
				} else
				{
					open_lin_error_handler(OPEN_LIN_MASTER_ERROR_DATA_TX);
					open_lin_master_set_state(OPEN_LIN_MASTER_IDLE);
				}
				break;
			}
			default:
				/* do nothing */
				break;
		}
	} else {
		/* empty master table do nothing */
	}
}

void open_lin_master_dl_set_state_callback(void (*callback)(t_open_lin_master_state new_state))
{
	lin_master_state_callback = callback;
}

l_u32 open_lin_master_dl_get_frame_start_time_us()
{
	return frame_start_time_us;
}
