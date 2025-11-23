/**
 ****************************************************************************************
 *
 * @file user_empty_peripheral_template.c
 *
 * @brief Empty peripheral template project source code.
 *
 * Copyright (C) 2012-2023 Renesas Electronics Corporation and/or its affiliates.
 * All rights reserved. Confidential Information.
 *
 * This software ("Software") is supplied by Renesas Electronics Corporation and/or its
 * affiliates ("Renesas"). Renesas grants you a personal, non-exclusive, non-transferable,
 * revocable, non-sub-licensable right and license to use the Software, solely if used in
 * or together with Renesas products. You may make copies of this Software, provided this
 * copyright notice and disclaimer ("Notice") is included in all such copies. Renesas
 * reserves the right to change or discontinue the Software at any time without notice.
 *
 * THE SOFTWARE IS PROVIDED "AS IS". RENESAS DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. TO THE
 * MAXIMUM EXTENT PERMITTED UNDER LAW, IN NO EVENT SHALL RENESAS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE, EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES. USE OF THIS SOFTWARE MAY BE SUBJECT TO TERMS AND CONDITIONS CONTAINED IN
 * AN ADDITIONAL AGREEMENT BETWEEN YOU AND RENESAS. IN CASE OF CONFLICT BETWEEN THE TERMS
 * OF THIS NOTICE AND ANY SUCH ADDITIONAL LICENSE AGREEMENT, THE TERMS OF THE AGREEMENT
 * SHALL TAKE PRECEDENCE. BY CONTINUING TO USE THIS SOFTWARE, YOU AGREE TO THE TERMS OF
 * THIS NOTICE.IF YOU DO NOT AGREE TO THESE TERMS, YOU ARE NOT PERMITTED TO USE THIS
 * SOFTWARE.
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup APP
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "rwip_config.h"             // SW configuration
#include "gattc_task.h"
#include "gap.h"
#include "app_api.h"
#include "app_easy_timer.h"
#include "co_bt.h"
#include "user_empty_peripheral_template.h"
#include "MCP9808.h"
#include "arch_console.h"

/*
 * TYPE DEFINITIONS
 ****************************************************************************************
 */

// Manufacturer Specific Data ADV structure type
struct mnf_specific_data_ad_structure
{
    uint8_t ad_structure_size;
    uint8_t ad_structure_type;
    uint8_t company_id[APP_AD_MSD_COMPANY_ID_LEN];
    uint8_t proprietary_data[APP_AD_MSD_DATA_LEN];
};

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */

timer_hnd app_adv_data_update_timer_used        __SECTION_ZERO("retention_mem_area0");
struct mnf_specific_data_ad_structure mnf_data  __SECTION_ZERO("retention_mem_area0");
uint8_t mnf_data_index                          __SECTION_ZERO("retention_mem_area0");
uint8_t stored_adv_data_len                     __SECTION_ZERO("retention_mem_area0");
uint8_t stored_adv_data[ADV_DATA_LEN]           __SECTION_ZERO("retention_mem_area0");

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
*/

/**
 ****************************************************************************************
 * @brief Initialize Manufacturer Specific Data
 * @return void
 ****************************************************************************************
 */
static void mnf_data_init()
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

    mnf_data.ad_structure_size = sizeof(struct mnf_specific_data_ad_structure) - sizeof(uint8_t);
    mnf_data.ad_structure_type = GAP_AD_TYPE_MANU_SPECIFIC_DATA;
    mnf_data.company_id[0] = APP_AD_MSD_COMPANY_ID & 0xFF;         // LSB
    mnf_data.company_id[1] = (APP_AD_MSD_COMPANY_ID >> 8) & 0xFF;  // MSB
    memset(mnf_data.proprietary_data, 0, APP_AD_MSD_DATA_LEN);
}

/**
 ****************************************************************************************
 * @brief Update Manufacturer Specific Data with temperature reading
 * @return void
 ****************************************************************************************
 */
static void mnf_data_update()
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

    uint8_t length;
    double temperature = MCP9808_get_temperature();

    int temp_int  = (int)temperature;
    int temp_frac = (int)((temperature - temp_int) * 10000);

    length = snprintf((char*)mnf_data.proprietary_data, TEMPERATURE_DATA, SNPRINT_FORMAT ,temp_int, temp_frac);
    mnf_data.ad_structure_size = sizeof(struct mnf_specific_data_ad_structure) - sizeof(uint8_t) - (APP_AD_MSD_DATA_LEN - length);

    arch_printf("Temperature: %d.%02d C\r\n", temp_int, temp_frac);
}

/**
 ****************************************************************************************
 * @brief Add an AD structure in the Advertising or Scan Response Data
 * @param[in] cmd               GAPM_START_ADVERTISE_CMD parameter struct
 * @param[in] ad_struct_data    AD structure buffer
 * @param[in] ad_struct_len     AD structure length
 * @param[in] adv_connectable   Connectable advertising event or not
 * @return void
 */
static void app_add_ad_struct(struct gapm_start_advertise_cmd *cmd, void *ad_struct_data, uint8_t ad_struct_len, uint8_t adv_connectable)
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

	// Append manufacturer data to advertising data
	memcpy(&cmd->info.host.adv_data[cmd->info.host.adv_data_len], ad_struct_data, ad_struct_len);
	cmd->info.host.adv_data_len += ad_struct_len;
	mnf_data_index = cmd->info.host.adv_data_len - sizeof(struct mnf_specific_data_ad_structure);

    // Store advertising and scan response data
    stored_adv_data_len = cmd->info.host.adv_data_len;
    memcpy(stored_adv_data, cmd->info.host.adv_data, stored_adv_data_len);
}

/**
 ****************************************************************************************
 * @brief Advertisement data update timer callback function.
 * @return void
 ****************************************************************************************
*/
static void adv_data_update_timer_cb()
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

    // Update manufacturer data with new temperature
    mnf_data_update();

    // Update the manufacturer data in the stored advertising/scan response data
    memcpy(stored_adv_data + (mnf_data_index & 0x7F), &mnf_data, sizeof(struct mnf_specific_data_ad_structure));

    // Update advertising data on the fly
    app_easy_gap_update_adv_data(stored_adv_data, stored_adv_data_len, NULL, 0);

    // Restart timer for next update
    app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);
}

void user_app_init(void)
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

    // Initialize Manufacturer Specific Data
    mnf_data_init();

    arch_printf("Added mfg data\r\n");

    // Initialize advertising and scan response data
    memcpy(stored_adv_data, USER_ADVERTISE_DATA, USER_ADVERTISE_DATA_LEN);
    stored_adv_data_len = USER_ADVERTISE_DATA_LEN;

    default_app_on_init();
}

void user_app_adv_start(void)
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

    // Schedule the next advertising data update
    app_adv_data_update_timer_used = app_easy_timer(APP_ADV_DATA_UPDATE_TO, adv_data_update_timer_cb);

    // Get current temperature and update manufacturer data
    mnf_data_update();

    struct gapm_start_advertise_cmd* cmd;
    cmd = app_easy_gap_non_connectable_advertise_get_active();

    // Add manufacturer data to advertising or scan response data
    app_add_ad_struct(cmd, &mnf_data, sizeof(struct mnf_specific_data_ad_structure), 1);

    app_easy_gap_non_connectable_advertise_start();
}

void user_app_adv_nonconn_complete(uint8_t status)
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

    // If advertising was canceled then update advertising data and start advertising again
    if (status == GAP_ERR_CANCELED)
    {
    	struct gapm_start_advertise_cmd *cmd = app_easy_gap_non_connectable_advertise_get_active();
    	app_easy_gap_non_connectable_advertise_start();
    }
}

void user_catch_rest_hndl(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
	arch_printf("\n\r%s\n\r", __FUNCTION__);

    switch(msgid)
    {
        case GATTC_EVENT_REQ_IND:
        {
            // Confirm unhandled indication to avoid GATT timeout
            struct gattc_event_ind const *ind = (struct gattc_event_ind const *) param;
            struct gattc_event_cfm *cfm = KE_MSG_ALLOC(GATTC_EVENT_CFM, src_id, dest_id, gattc_event_cfm);
            cfm->handle = ind->handle;
            KE_MSG_SEND(cfm);
        } break;

        default:
            break;
    }
}

/// @} APP
