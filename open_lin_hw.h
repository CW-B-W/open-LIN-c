/**
 * @file open_lin_hw.h
 * @author LJ
 * @date 25.01.2018
 * @brief LIN hardware function calls
 *
 */

#ifndef OPEN_LIN_OPEN_LIN_HW_H_
#define OPEN_LIN_OPEN_LIN_HW_H_

#include "open_lin_types.h"
/**
 * @brief Checks if break signal occurred on UART
 *
 * @return @c true is break is detected
 */
l_bool open_lin_hw_check_for_break(void);

/**
 * @brief resets hardware to default state
 *
 * @return @c true is break is detected
 */
void open_lin_hw_reset(void);

void open_lin_set_rx_enabled(l_bool status);

void open_lin_hw_init(void);

l_u32 open_lin_hw_get_time_us();

l_u8 open_lin_hw_tx_data(l_u8* data, l_u8 len);
l_u8 open_lin_hw_rx_data(l_u8 *data, l_u8 len, l_u32 timeout_us);

l_bool open_lin_hw_tx_break(void);

l_bool open_lin_hw_tx_byte(l_u8 data);
l_bool open_lin_hw_rx_byte(l_u8 *data, l_u32 timeout_us);

#ifdef OPEN_LIN_AUTO_BAUD
/**
 * @brief Starts auto baud procedure
 *
 */
void open_lin_hw_set_auto_baud(void);

#endif /* OPEN_LIN_AUTO_BAUD */

#endif /* OPEN_LIN_OPEN_LIN_HW_H_ */
