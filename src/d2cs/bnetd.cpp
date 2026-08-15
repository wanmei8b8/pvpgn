/*
 * Copyright (C) 2000,2001	Onlyer	(onlyer@263.net)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "common/setup_before.h"
#include "setup.h"
#include "bnetd.h"

#include <ctime>

#include "common/eventlog.h"
#include "prefs.h"
#include "s2s.h"
#include "handle_bnetd.h"
#include "common/setup_after.h"

static const char* conn_state_to_str(t_conn_state state) {
    switch (state) {
        case conn_state_none:         return "NONE";
        case conn_state_init:         return "INIT";
        case conn_state_connecting:   return "CONNECTING";
        case conn_state_connected:    return "CONNECTED";
        case conn_state_authed:       return "AUTHED";
        case conn_state_char_authed:  return "CHAR_AUTHED";
        case conn_state_destroy:      return "DESTROY";
        case conn_state_destroying:   return "DESTROYING";
        case conn_state_any:          return "ANY";
        default:                      return "UNKNOWN";
    }
}
namespace pvpgn
{

	namespace d2cs
	{

		static t_connection * bnetd_connection = NULL;

		extern int bnetd_init(void)
		{
			return bnetd_check();
		}

		extern int bnetd_check(void)
		{
			static std::time_t prev_connecting_checktime = 0;

			if (bnetd_connection) {
                t_conn_state state = d2cs_conn_get_state(bnetd_connection);
				if (state == conn_state_connecting) {
					if (std::time(nullptr) - prev_connecting_checktime > prefs_get_s2s_timeout()) {
						eventlog(eventlog_level_warn, __FUNCTION__, "connection to bnetd s2s timeout");
						d2cs_conn_set_state(bnetd_connection, conn_state_destroy);
						return -1;
					}
				}  if (state != conn_state_connecting &&
                    state != conn_state_connected &&
                    state != conn_state_authed &&
                    state != conn_state_char_authed) {

                    // --- 进入异常处理分支 ---

                    // A. 打印可读的异常状态
                    eventlog(eventlog_level_warn, __FUNCTION__, "bnetd connection in bad state: {}",
                        conn_state_to_str(state));

                    // B. 安全销毁：标记状态触发链表清理，避免直接 free 导致的竞态
                    d2cs_conn_set_state(bnetd_connection, conn_state_destroy);

                    // C. 关键：本地指针置空，强制下次循环重新创建连接
                    bnetd_connection = NULL;

                    return -1;
                }
				return 0;
			}
			if (!(bnetd_connection = s2s_create(prefs_get_bnetdaddr(), BNETD_SERV_PORT, conn_class_bnetd))) {
				return -1;
			}
			if (d2cs_conn_get_state(bnetd_connection) == conn_state_init) {
				handle_bnetd_init(bnetd_connection);
			}
			else {
				prev_connecting_checktime = std::time(nullptr);
			}
			return 0;
		}


		extern t_connection * bnetd_conn(void)
		{
			return bnetd_connection;
		}

		extern int bnetd_set_connection(t_connection * c)
		{
			bnetd_connection = c;
			return 0;
		}

		extern int bnetd_destroy(t_connection * c)
		{
			if (bnetd_connection != c) {
				eventlog(eventlog_level_error, __FUNCTION__, "bnetd connection do not match");
				return -1;
			}
			bnetd_connection = NULL;
			return 0;
		}


	}

}
