/* encoding UTF-8 */

/*
 * This file is part of the "Verilog Lua" distribution (https://github.com/yrasik/Verilog_Lua).
 * Copyright (c) 2022 Yuri Stepanenko.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
/**
  ******************************************************************************
  * @file    PLI2Lua.c
  * @author  Stepanenko Yuri
  * @version V2.1.0a
  * @date    9-May-2022
  * @brief   PLI интерфейс для Lua
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Stepanenko Yuri</center></h2>
  *
  *
  ******************************************************************************
  */


/**
  * @mainpage Часть проекта интерфейса между языком описания аппаратуры Verilog и языком программирования Lua.
  * \n
  * Предполагается, что на Lua проще написать модель, эмулирующую ПО микроконтроллера ( функцию main() и даже обработчик прерывания irq() ).
  * Как вариант, на Lua можно поднять сетевой интерфейс (использовать luasocket-3.0-rc1) и отправлять/принимать данные по сети и скармливать их Verilog - модели.
  * \n
  * На Lua можно создать wawe-форму и скормить её verilog- модели. И наоборот...
  * \n
  * Исходный файл тестировался с Icarus verilog-10.3 и Modelsim
  *
  * ~~~~~~~~~~~~~~~{.lua}
  * function init_env()
  *   print('<------- init_env ------>')
  *   return 1
  * end
  * ~~~~~~~~~~~~~~~
  *
  * ~~~~~~~~~~~~~~~{.lua}
  * function exchange_M(DAT_I, STATUS_I)
  *   print('<------- exchange_M ------>')
  *   CMD_O = 0
  *   ADR_O = 1
  *   DAT_O = 2
  *   return CMD_O, ADR_O, DAT_O
  * end
  * ~~~~~~~~~~~~~~~
  *
  *
  *
  * ~~~~~~~~~~~~~~~{.v}
  * reg [63:0] Descriptor;
  * reg [31:0] cmd, rr, cnt;
  *
  * reg [31:0] A;
  * reg [31:0] Dw;
  * reg [31:0] Dr;
  * 
  * initial begin
  *   $lua_init(lua_script_name, Descriptor[63:32], Descriptor[31:0]);
  *   if(Descriptor == 0)
  *     begin
  *       $display("ERROR : init lua script");
  *       $finish;
  *     end
  *
  *     Phase = ST__IDLE;
  *     Dr = 'hFFFFFFFF;
  * end
  * 
  * always@(posedge CLK_I)
  *   if(RST_I)
  *     begin
  *       Phase <= #1 ST__IDLE;
  *       UP = 1'b0;
  *     end
  *   else
  *     begin
  *       Phase <= #1 NewPhase;
  *
  *       if(Phase == ST__EXCHANGE)
  *         begin
  *          #1
  *          UP = 1'b1;
  *          prev_time_ns = time_ns;
  *          $lua_exchange_M(Descriptor[63:32], Descriptor[31:0], 
  *             time_ns, cmd, A, Dw, Dr, {31'h0, IRQ}, rr);
  *          $display("time_ns = %d", time_ns);
  *         end
  *       else
  *         begin
  *          #1
  *          UP = 1'b0;
  *         end
  *     end
  * 
  * ~~~~~~~~~~~~~~~
  */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>


#include "veriuser.h"
#include "vpi_user.h"
#include "acc_user.h"


#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"


#define DEBUG
#define PFX  __FILE__
//#define _FD_  s->log_file
#define _FD_  stdout
#include "debug.h"


enum
{
  ACTION__IDLE   = 0,
  ACTION__READ   = 1,
  ACTION__WRITE  = 2
} typedef action_t;


struct {
  lua_State *L;

} typedef mb_lua_t;


/**
  * @brief Инициализация Lua - машины.
  * @param  fname: Ссылка на строку с именем файла Lua - программы.
  * @retval void* Указатель на объект lua_State, приведённый к void*. В случае неудачи возвращает NULL.
  */
static uint64_t init_lua(mb_lua_t **master_, const char *fname)
{
  int       err;
  lua_Integer ret;
  mb_lua_t *master;

  master = (mb_lua_t *)malloc(sizeof(mb_lua_t));

  if (master == NULL) {
    REPORT(MSG_ERROR, "if (master == NULL)");
    return -1;
  }

  master->L = NULL;
  master->L = luaL_newstate();

  if( master->L == NULL )
  {
    REPORT(MSG_ERROR, "if( master->L == NULL )");
    free(master);
    *master_ = NULL;
    return -2;
  }

  luaL_openlibs( master->L );

  err = luaL_loadfile( master->L, fname );
  if ( err != LUA_OK )
  {
    REPORT(MSG_ERROR, "if ( err != LUA_OK )  '%s' filename = '%s'", lua_tostring(master->L, -1), fname);
    lua_close( master->L );
    free(master);
    *master_ = NULL;
    return -3;
  }

  if( lua_pcall(master->L, 0, 0, 0) != LUA_OK )
  {
    REPORT(MSG_ERROR, "if( lua_pcall(master->L, 0, 0, 0) != LUA_OK )  '%s'", lua_tostring(master->L, -1));
    lua_close( master->L );
    free(master);
    *master_ = NULL;
    return -4;
  }

  lua_getglobal(master->L, "init_env");

  if( lua_pcall(master->L, 0, 1, 0) != LUA_OK )
  {
    REPORT(MSG_ERROR, "if( lua_pcall(master->L, 0, 1, 0) != LUA_OK )  '%s'", lua_tostring(master->L, -1));
    lua_close( master->L );
    free(master);
    *master_ = NULL;
    return -5;
  }

  if(! lua_isinteger(master->L, -1))
  {
    REPORT(MSG_ERROR, "if(! lua_isinteger(master->L, -1))  '%s'", lua_tostring(master->L, -1));
    lua_close( master->L );
    free(master);
    *master_ = NULL;
    return -6;
  }

  ret = lua_tointeger(master->L, -1);
  lua_pop(master->L, 1);

  if( ret < 0 )
  {
    REPORT(MSG_ERROR, "if( ret < 0 )");
    lua_close( master->L );
    free(master);
    *master_ = NULL;
    return 0;
  }

  REPORT(MSG_INFO, "Descriptor = 0x%llX", (uint64_t)master );
  *master_ = master;
  return 0;
}


static void deinit_lua(mb_lua_t *master)
{
  REPORT(MSG_INFO, "Descriptor = 0x%llX", (uint64_t)master );
  if( master == NULL)
  {
    REPORT(MSG_INFO, "if( master == NULL)");
    return;
  }

  lua_close( master->L );
  free(master);
}


/**
  * @brief Обмен данными, приспособленный под интерфейс системной шины процессора.
  * @param  desc:  Указатель на Lua - машину.
  * @param  CMD_O: Возвращает код команды (ожидание, запись, чтение). Эта команда используется автоматом состояний, написанном на Verilog для отработки соответствующей временной диаграмы на шине данных.
  * @param  ADR_O: Возвращает адрес (32 бита) для формирования на шине адреса.
  * @param  DAT_O: Возвращает данные (32 бита) для формирования на шине данных.
  * @param  DAT_I: Считывает данные (32 бита), принимаемые по шине данных.
  * @param  STATUS_I: Считывает состояния сигналов сброса и прерываний (32 бита, рекомендуется в 31 бит помещать состояние сигнала сброса.)
  * @retval int32_t Возвращает 0 в случае успеха, отрицательные величины в случае неудачи.
  */
static int lua_exchange_M(mb_lua_t *master, int32_t *time_ns, int32_t *CMD_O, int32_t *ADR_O, int32_t *DAT_O, const int32_t *DAT_I, const int32_t *STATUS_I)
{
  printf("<----------------- lua_exchange_M ---------------->\n");

  if(master == NULL)
  {
    REPORT(MSG_ERROR, "if(master == NULL)");
    return -1;
  }

  lua_getglobal(master->L, "exchange_M");

  lua_pushinteger(master->L, *DAT_I);
  lua_pushinteger(master->L, *STATUS_I);

  if( lua_pcall(master->L, 2, 4, 0) != LUA_OK )
  {
    REPORT(MSG_ERROR, "if( lua_pcall(master->L, 2, 4, 0) != LUA_OK )  '%s'", lua_tostring(master->L, -1));
    return -2;
  }

#ifdef DEBUG
  if(! lua_isinteger(master->L, -4))
  {
     REPORT(MSG_ERROR, "if(! lua_isinteger(master->L, -4))  '%s'", lua_tostring(master->L, -1));
     return -3;
  }
#endif
  *time_ns = (uint32_t)lua_tointeger(master->L, -4);

#ifdef DEBUG
  if(! lua_isinteger(master->L, -3))
  {
     REPORT(MSG_ERROR, "if(! lua_isinteger(master->L, -3))  '%s'", lua_tostring(master->L, -1));
     return -4;
  }
#endif
  *CMD_O = (uint32_t)lua_tointeger(master->L, -3);

#ifdef DEBUG
  if(! lua_isinteger(master->L, -2))
  {
     REPORT(MSG_ERROR, "if(! lua_isinteger(L, -2))  '%s'", lua_tostring(master->L, -1));
     return -5;
  }
#endif
  *ADR_O = (uint32_t)lua_tointeger(master->L, -2);

#ifdef DEBUG
  if(! lua_isinteger(master->L, -1))
  {
     REPORT(MSG_ERROR, "if(! lua_isinteger(L, -1))  '%s'", lua_tostring(master->L, -1));
     return -6;
  }
#endif
  *DAT_O = (uint32_t)lua_tointeger(master->L, -1);

  lua_pop(master->L, 4);
  return 0;
}


static int lua_exchange_S(mb_lua_t *slave, const int32_t *time_ns, const int32_t *CMD_I, const int32_t *ADR_I, const int32_t *DAT_I, int32_t *DAT_O, int32_t *STATUS_O)
{
  printf("<----------------- lua_exchange_S ---------------->\n");

  if(slave == NULL)
  {
    REPORT(MSG_ERROR, "if(slave == NULL)");
    return -1;
  }

  lua_getglobal(slave->L, "exchange_S");

  lua_pushinteger(slave->L, *time_ns);
  lua_pushinteger(slave->L, *CMD_I);
  lua_pushinteger(slave->L, *ADR_I);
  lua_pushinteger(slave->L, *DAT_I);

  if( lua_pcall(slave->L, 4, 2, 0) != LUA_OK )
  {
    REPORT(MSG_ERROR, "if( lua_pcall(slave->L, 4, 2, 0) != LUA_OK )  '%s'", lua_tostring(slave->L, -1));
    return -2;
  }

#ifdef DEBUG
  if(! lua_isinteger(slave->L, -2))
  {
     REPORT(MSG_ERROR, "if(! lua_isinteger(slave->L, -2))  '%s'", lua_tostring(slave->L, -1));
     return -3;
  }
#endif
  *DAT_O = (uint32_t)lua_tointeger(slave->L, -2);

#ifdef DEBUG
  if(! lua_isinteger(slave->L, -1))
  {
     REPORT(MSG_ERROR, "if(! lua_isinteger(master->L, -1))  '%s'", lua_tostring(slave->L, -1));
     return -4;
  }
#endif
  *STATUS_O = (uint32_t)lua_tointeger(slave->L, -1);

  lua_pop(slave->L, 2);
  return 0;
}


/**
  * @brief PLI - обёртка для функции init_lua(const char *fname)
  */
static PLI_INT32 calltf_lua_init(PLI_BYTE8 *user_data)
{
  const char *fname;
  vpiHandle inst_h;
  vpiHandle arg_iter;
  vpiHandle fname_hdl;
  vpiHandle descriptor_hdl_LO;
  vpiHandle descriptor_hdl_HI;
  s_vpi_value value_s;
  uint64_t descriptor;
  int ret;
  mb_lua_t *master = NULL;


  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

  if(arg_iter == NULL)
  {
    REPORT(MSG_ERROR, "if(arg_iter == NULL)");
    return 0;
  }

  descriptor_hdl_LO = vpi_scan(arg_iter);

  if(descriptor_hdl_LO == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_LO) == NULL)");
    return 0;
  }

  descriptor_hdl_HI = vpi_scan(arg_iter);

  if(descriptor_hdl_HI == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_HI) == NULL)");
    return 0;
  }

  fname_hdl = vpi_scan(arg_iter);

  if(fname_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(fname_hdl) == NULL)");
    return 0;
  }

  value_s.format = vpiStringVal;
  vpi_get_value(fname_hdl, &value_s);
  fname = value_s.value.str;

  if(fname == NULL)
  {
    REPORT(MSG_ERROR, "if(fname == NULL)");
    return 0;
  }

  ret = init_lua(&master, fname);
  if( ret < 0 )
  {
    REPORT(MSG_ERROR, "if( ret < 0 )");
    return 0;
  }

  descriptor = (uint64_t)(master);

  if(descriptor == 0)
  {
    REPORT(MSG_ERROR, "if(descriptor == NULL)");
    return 0;
  }


  value_s.format = vpiIntVal;
  value_s.value.integer = (PLI_INT32)descriptor;
  vpi_put_value(descriptor_hdl_LO, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = (PLI_INT32)(descriptor >> 32);
  vpi_put_value(descriptor_hdl_HI, &value_s, NULL, vpiNoDelay);

  vpi_free_object(fname_hdl);
  vpi_free_object(descriptor_hdl_LO);
  vpi_free_object(descriptor_hdl_HI);

  vpi_free_object(arg_iter);
  vpi_free_object(inst_h);

  return 0;
}


/**
  * @brief PLI - обёртка для функции void deinit_sockets(mb_master_t *master)
  */
static PLI_INT32 calltf_lua_deinit(PLI_BYTE8 *user_data)
{
  vpiHandle inst_h;
  vpiHandle arg_iter;

  vpiHandle descriptor_hdl_LO;
  vpiHandle descriptor_hdl_HI;

  s_vpi_value value_s;

  uint64_t descriptor;


  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

#ifdef DEBUG
  if(arg_iter == NULL)
  {
    REPORT(MSG_ERROR, "if(arg_iter == NULL)");
    return 0;
  }
#endif

  descriptor_hdl_LO = vpi_scan(arg_iter);

#ifdef DEBUG
  if(descriptor_hdl_LO == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_LO) == NULL)");
    return 0;
  }
#endif

  descriptor_hdl_HI = vpi_scan(arg_iter);

#ifdef DEBUG
  if(descriptor_hdl_HI == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_HI) == NULL)");
    return 0;
  }
#endif


  value_s.format = vpiIntVal;
  vpi_get_value(descriptor_hdl_HI, &value_s);
  descriptor = (uint64_t)(value_s.value.integer);
  descriptor = descriptor << 32;

  vpi_get_value(descriptor_hdl_LO, &value_s);
  descriptor |= (0x00000000FFFFFFFF & (uint64_t)(value_s.value.integer));

  deinit_lua((mb_lua_t *)descriptor);

  vpi_free_object(descriptor_hdl_HI);
  vpi_free_object(descriptor_hdl_LO);

  vpi_free_object(arg_iter);
  vpi_free_object(inst_h);
  return 0;

}


/**
  * @brief PLI - обёртка для функции exchange_CAD(lua_State* L, int32_t *CMD_O, int32_t *ADR_O, int32_t *DAT_O, const int32_t *DAT_I, const int32_t *STATUS_I)
  */
static PLI_INT32 calltf_lua_exchange_M(PLI_BYTE8 *user_data) //Verilog -> lua
{
  vpiHandle inst_h;
  vpiHandle arg_iter;

  vpiHandle descriptor_hdl_LO;
  vpiHandle descriptor_hdl_HI;
  vpiHandle time_ns_hdl;
  vpiHandle CMD_O_hdl;
  vpiHandle ADR_O_hdl;
  vpiHandle DAT_O_hdl;
  vpiHandle DAT_I_hdl;
  vpiHandle STATUS_I_hdl;
  vpiHandle result_hdl;

  s_vpi_value value_s;

  uint64_t descriptor;
  int32_t time_ns = 0;
  int32_t CMD_O = 0;
  int32_t ADR_O = 0;
  int32_t DAT_O = 0;
  int32_t DAT_I;
  int32_t STATUS_I;
  int32_t result;

  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

#ifdef DEBUG
  if(arg_iter == NULL)
  {
    REPORT(MSG_ERROR, "if(arg_iter == NULL)");
    return 0;
  }
#endif

  descriptor_hdl_LO = vpi_scan(arg_iter);

#ifdef DEBUG
  if(descriptor_hdl_LO == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_LO) == NULL)");
    return 0;
  }
#endif

  descriptor_hdl_HI = vpi_scan(arg_iter);

#ifdef DEBUG
  if(descriptor_hdl_HI == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_HI) == NULL)");
    return 0;
  }
#endif

  time_ns_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(time_ns_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(time_ns_hdl == NULL)");
    return 0;
  }
#endif

  CMD_O_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(CMD_O_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(CMD_O_hdl) == NULL)");
    return 0;
  }
#endif

  ADR_O_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(ADR_O_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(ADR_O_hdl) == NULL)");
    return 0;
  }
#endif

  DAT_O_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(DAT_O_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(DAT_O_hdl) == NULL)");
    return 0;
  }
#endif

  DAT_I_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(DAT_I_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(DAT_I_hdl) == NULL)");
    return 0;
  }
#endif

  STATUS_I_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(STATUS_I_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(STATUS_I_hdl) == NULL)");
    return 0;
  }
#endif

  result_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(result_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(result_hdl) == NULL)");
    return 0;
  }
#endif

  value_s.format = vpiIntVal;
  vpi_get_value(descriptor_hdl_HI, &value_s);
  descriptor = (uint64_t)(value_s.value.integer);
  descriptor = descriptor << 32;

  vpi_get_value(descriptor_hdl_LO, &value_s);
  descriptor |= (0x00000000FFFFFFFF & (uint64_t)(value_s.value.integer));


  vpi_get_value(DAT_I_hdl, &value_s);
  DAT_I = value_s.value.integer;

  vpi_get_value(STATUS_I_hdl, &value_s);
  STATUS_I = value_s.value.integer;

  result = lua_exchange_M((mb_lua_t *)descriptor, &time_ns, &CMD_O, &ADR_O, &DAT_O, &DAT_I, &STATUS_I);

  value_s.value.integer = time_ns;
  vpi_put_value(time_ns_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = CMD_O;
  vpi_put_value(CMD_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = ADR_O;
  vpi_put_value(ADR_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = DAT_O;
  vpi_put_value(DAT_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = result;
  vpi_put_value(result_hdl, &value_s, NULL, vpiNoDelay);

  vpi_free_object(descriptor_hdl_HI);
  vpi_free_object(descriptor_hdl_LO);
  vpi_free_object(time_ns_hdl);
  vpi_free_object(CMD_O_hdl);
  vpi_free_object(ADR_O_hdl);
  vpi_free_object(DAT_O_hdl);
  vpi_free_object(DAT_I_hdl);
  vpi_free_object(STATUS_I_hdl);
  vpi_free_object(result_hdl);

  vpi_free_object(arg_iter);
  vpi_free_object(inst_h);

  return 0;
}


static PLI_INT32 calltf_lua_exchange_S(PLI_BYTE8 *user_data) //Verilog -> lua
{
  vpiHandle inst_h;
  vpiHandle arg_iter;

  vpiHandle descriptor_hdl_LO;
  vpiHandle descriptor_hdl_HI;
  vpiHandle time_ns_hdl;
  vpiHandle CMD_I_hdl;
  vpiHandle ADR_I_hdl;
  vpiHandle DAT_I_hdl;
  vpiHandle DAT_O_hdl;
  vpiHandle STATUS_O_hdl;
  vpiHandle result_hdl;

  s_vpi_value value_s;

  uint64_t descriptor;
  int32_t time_ns;
  int32_t CMD_I;
  int32_t ADR_I;
  int32_t DAT_I;
  int32_t DAT_O = 0;
  int32_t STATUS_O = 0;
  int32_t result;

  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

#ifdef DEBUG
  if(arg_iter == NULL)
  {
    REPORT(MSG_ERROR, "if(arg_iter == NULL)");
    return 0;
  }
#endif

  descriptor_hdl_LO = vpi_scan(arg_iter);

#ifdef DEBUG
  if(descriptor_hdl_LO == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_LO) == NULL)");
    return 0;
  }
#endif

  descriptor_hdl_HI = vpi_scan(arg_iter);

#ifdef DEBUG
  if(descriptor_hdl_HI == NULL)
  {
    REPORT(MSG_ERROR, "if(descriptor_hdl_HI) == NULL)");
    return 0;
  }
#endif

  time_ns_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(time_ns_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(time_ns_hdl == NULL)");
    return 0;
  }
#endif

  CMD_I_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(CMD_I_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(CMD_O_hdl) == NULL)");
    return 0;
  }
#endif

  ADR_I_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(ADR_I_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(ADR_O_hdl) == NULL)");
    return 0;
  }
#endif

  DAT_I_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(DAT_I_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(DAT_O_hdl) == NULL)");
    return 0;
  }
#endif

  DAT_O_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(DAT_O_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(DAT_I_hdl) == NULL)");
    return 0;
  }
#endif

  STATUS_O_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(STATUS_O_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(STATUS_I_hdl) == NULL)");
    return 0;
  }
#endif

  result_hdl = vpi_scan(arg_iter);

#ifdef DEBUG
  if(result_hdl == NULL)
  {
    REPORT(MSG_ERROR, "if(result_hdl) == NULL)");
    return 0;
  }
#endif

  value_s.format = vpiIntVal;
  vpi_get_value(descriptor_hdl_HI, &value_s);
  descriptor = (uint64_t)(value_s.value.integer);
  descriptor = descriptor << 32;

  vpi_get_value(descriptor_hdl_LO, &value_s);
  descriptor |= (0x00000000FFFFFFFF & (uint64_t)(value_s.value.integer));

  vpi_get_value(time_ns_hdl, &value_s);
  time_ns = value_s.value.integer;

  vpi_get_value(CMD_I_hdl, &value_s);
  CMD_I = value_s.value.integer;

  vpi_get_value(ADR_I_hdl, &value_s);
  ADR_I = value_s.value.integer;

  vpi_get_value(DAT_I_hdl, &value_s);
  DAT_I = value_s.value.integer;

  result = lua_exchange_S((mb_lua_t *)descriptor, &time_ns, &CMD_I, &ADR_I, &DAT_I, &DAT_O, &STATUS_O);


  value_s.value.integer = DAT_O;
  vpi_put_value(DAT_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = STATUS_O;
  vpi_put_value(STATUS_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = result;
  vpi_put_value(result_hdl, &value_s, NULL, vpiNoDelay);

  vpi_free_object(descriptor_hdl_HI);
  vpi_free_object(descriptor_hdl_LO);
  vpi_free_object(time_ns_hdl);
  vpi_free_object(CMD_I_hdl);
  vpi_free_object(ADR_I_hdl);
  vpi_free_object(DAT_I_hdl);
  vpi_free_object(DAT_O_hdl);
  vpi_free_object(STATUS_O_hdl);
  vpi_free_object(result_hdl);

  vpi_free_object(arg_iter);
  vpi_free_object(inst_h);

  return 0;
}


static int adderSizetf(char* user_data)
{
  return 0;
}

/**
  * @brief PLI - регистратор функций в Verilog - окружении.
  */
void vpit_RegisterTfs_Lua( void )
{
#if 1
  s_vpi_systf_data systf_data;
  vpiHandle systf_handle;

  systf_data.type = vpiSysTask;
  systf_data.sysfunctype = 0;
  systf_data.tfname = "$lua_init";
  systf_data.calltf = calltf_lua_init;
  systf_data.compiletf = 0;
  systf_data.sizetf = 0;
  systf_data.user_data = 0;
  systf_handle = vpi_register_systf(&systf_data);
  vpi_free_object(systf_handle);

  systf_data.type = vpiSysTask;
  systf_data.sysfunctype = 0;
  systf_data.tfname = "$lua_exchange_M";
  systf_data.calltf = calltf_lua_exchange_M;
  systf_data.compiletf = 0;
  systf_data.sizetf = 0;
  systf_data.user_data = 0;
  systf_handle = vpi_register_systf(&systf_data);
  vpi_free_object(systf_handle);

  systf_data.type = vpiSysTask;
  systf_data.sysfunctype = 0;
  systf_data.tfname = "$lua_exchange_S";
  systf_data.calltf = calltf_lua_exchange_S;
  systf_data.compiletf = 0;
  systf_data.sizetf = 0;
  systf_data.user_data = 0;
  systf_handle = vpi_register_systf(&systf_data);
  vpi_free_object(systf_handle);

  systf_data.type = vpiSysTask;
  systf_data.sysfunctype = 0;
  systf_data.tfname = "$lua_deinit";
  systf_data.calltf = calltf_lua_deinit;
  systf_data.compiletf = 0;
  systf_data.sizetf = 0;
  systf_data.user_data = 0;
  systf_handle = vpi_register_systf(&systf_data);
  vpi_free_object(systf_handle);

#else
  //FIXME  ХЗ как лучше: как функцию или как задачу. На функцию ругается, что переменных не возвращает...
  s_vpi_systf_data tf_data;

  tf_data.type        = vpiSysFunc;
  tf_data.sysfunctype = vpiSysFuncSized;
  tf_data.tfname      = "$lua_init";
  tf_data.calltf      = calltf_lua_init;
  tf_data.compiletf   = NULL;
  tf_data.sizetf      = adderSizetf;
  vpi_register_systf(&tf_data);

  tf_data.type        = vpiSysFunc;
  tf_data.sysfunctype = vpiSysFuncSized;
  tf_data.tfname      = "$lua_exchange_M";
  tf_data.calltf      = calltf_lua_exchange_M;
  tf_data.compiletf   = NULL;
  tf_data.sizetf      = adderSizetf;
  vpi_register_systf(&tf_data);

  tf_data.type        = vpiSysFunc;
  tf_data.sysfunctype = vpiSysFuncSized;
  tf_data.tfname      = "$lua_exchange_S";
  tf_data.calltf      = calltf_lua_exchange_S;
  tf_data.compiletf   = NULL;
  tf_data.sizetf      = adderSizetf;
  vpi_register_systf(&tf_data);

  tf_data.type        = vpiSysFunc;
  tf_data.sysfunctype = vpiSysFuncSized;
  tf_data.tfname      = "$lua_deinit";
  tf_data.calltf      = calltf_lua_deinit;
  tf_data.compiletf   = NULL;
  tf_data.sizetf      = adderSizetf;
  vpi_register_systf(&tf_data);

#endif
}

/*****************************************************************************
 *
 * Required structure for initializing VPI routines.
 *
 *****************************************************************************/

void (*vlog_startup_routines[])() = {
  vpit_RegisterTfs_Lua,
  0
};

/*****************************************************************************/
