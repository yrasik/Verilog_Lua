/* encoding UTF-8 */

/*
 * This file is part of the "Verilog Lua" distribution (https://github.com/yrasik/Verilog_Lua).
 * Copyright (c) 2020 Yuri Stepanenko.
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
  * @version V2.0.0a
  * @date    9-May-2020
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
  * function exchange_CAD(DAT_I, STATUS_I)
  *   print('<------- exchange_CAD ------>')
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
  * initial begin
  *   $init_Lua(lua_script_name, Descriptor);
  *   if(Descriptor == 0)
  *     begin
  *       $display("ERROR : init lua script");
  *       $stop;
  *     end
  *
  *     Phase = ST__IDLE;//FIXME
  *     Dr = 'hFFFFFFFF;
  * end
  * ~~~~~~~~~~~~~~~
  */


#include "veriuser.h"
#include "vpi_user.h"
#include "acc_user.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "lua.h"
#include "lauxlib.h"


/**
  * @brief Инициализация Lua - машины.
  * @param  fname: Ссылка на строку с именем файла Lua - программы.
  * @retval void* Указатель на объект lua_State, приведённый к void*. В случае неудачи возвращает NULL.
  */
static void* init_lua(const char *fname)
{
  int       err;

  lua_State *L = NULL;

  L = luaL_newstate();

  if( L == NULL)
  {
    return NULL;
  }

  luaL_openlibs( L );

  err = luaL_loadfile( L, fname );
  if ( err != LUA_OK )
  {
    lua_close( L );
    printf("ERROR : if ( err != LUA_OK )\n");
    return NULL;
  }


  lua_pcall(L, 0, 0, 0);
  lua_getglobal(L, "init_env");

  if( lua_pcall(L, 0, 1, 0) != LUA_OK )
  {
    printf("ERROR : in 'init_env()'  '%s'\n", lua_tostring(L, -1));
  }

  if(! lua_isnumber(L, -1))
  {
     printf("ERROR : in return type from 'init_env()'  '%s'\n", lua_tostring(L, -1));
  }

  double ret;
  ret = lua_tonumber(L, -1);
  lua_pop(L, 1);

  return (void*)L;
}



/**
  * @brief Обмен данными, приспособленный под интерфейс системной шины процессора.
  * @param  L: Указатель на Lua - машину.
  * @param  CMD_O: Возвращает код команды (ожидание, запись, чтение). Эта команда используется автоматом состояний, написанном на Verilog для отработки соответствующей временной диаграмы на шине данных.
  * @param  ADR_O: Возвращает адрес (32 бита) для формирования на шине адреса.
  * @param  DAT_O: Возвращает данные (32 бита) для формирования на шине данных.
  * @param  DAT_I: Считывает данные (32 бита), принимаемые по шине данных.
  * @param  STATUS_I: Считывает состояния сигналов сброса и прерываний (32 бита, рекомендуется в 31 бит помещать состояние сигнала сброса.)
  * @retval int32_t Возвращает 0 в случае успеха, отрицательные величины в случае неудачи.
  */
static int32_t exchange_CAD(lua_State* L, int32_t *CMD_O, int32_t *ADR_O, int32_t *DAT_O, const int32_t *DAT_I, const int32_t *STATUS_I)
{
  lua_getglobal(L, "exchange_CAD");

  lua_pushinteger(L, *DAT_I);
  lua_pushinteger(L, *STATUS_I);

  if( lua_pcall(L, 2, 3, 0) != LUA_OK )
  {
    printf("ERROR : in 'exchange_CAD()'  '%s'\n", lua_tostring(L, -1));
    return -1;
  }


  if(! lua_isinteger(L, 1))
  {
     printf("ERROR : in return type from 'exchange_CAD()'  '%s'\n", lua_tostring(L, -1));
     return -2;
  }
  *CMD_O = (uint32_t)lua_tointeger(L, 1);


  if(! lua_isinteger(L, 2))
  {
     printf("ERROR : in return type from 'exchange_CAD()'  '%s'\n", lua_tostring(L, -1));
     return -3;
  }
  *ADR_O = (uint32_t)lua_tointeger(L, 2);


  if(! lua_isinteger(L, 3))
  {
     printf("ERROR : in return type from 'exchange_CAD()'  '%s'\n", lua_tostring(L, -1));
     return -4;
  }

  *DAT_O = (uint32_t)lua_tointeger(L, 3);

  lua_pop(L, 3);//FIXME ?

  return 0;
}


/**
  * @brief Чтение данных из Lua.
  * @param  L: Указатель на Lua - машину.
  * @param  CMD_I: Команда: 0 - сброс источника, 1 - работа.
  * @param  DAT_O: Возвращает данные (32 бита) для записи в поток данных Verilog.
  * @retval int32_t Возвращает 0 в случае успеха, отрицательные величины в случае неудачи.
  */
static int32_t read_data(lua_State* L, const int32_t *CMD_I, int32_t *DAT_O)  // CMD_I -> lua -> DAT_O
{
  lua_getglobal(L, "read_data");

  lua_pushinteger(L, *CMD_I);

  if( lua_pcall(L, 1, 1, 0) != LUA_OK )
  {
    printf("ERROR : in 'read_data()'  '%s'\n", lua_tostring(L, -1));
    return -1;
  }

  if(! lua_isinteger(L, 1))
  {
     printf("ERROR : in return type from 'read_data()'  '%s'\n", lua_tostring(L, -1));
     return -4;
  }

  *DAT_O = (uint32_t)lua_tointeger(L, 1);

  lua_pop(L, 1);//FIXME ?

  return 0;
}


/**
  * @brief Запись данных в Lua.
  * @param  L: Указатель на Lua - машину.
  * @param  TIME_I: временная метка для передачи в Lua.
  * @param  DAT_I: Считывает данные (32 бита) для передачи в Lua.
  * @retval int32_t Возвращает 0 в случае успеха, отрицательные величины в случае неудачи.
  */
static int32_t write_data(lua_State* L, const int32_t *TIME_I, const int32_t *DAT_I) // TIME_I, DAT_I -> lua
{
  lua_getglobal(L, "write_data");

  lua_pushinteger(L, *TIME_I);
  lua_pushinteger(L, *DAT_I);

  if( lua_pcall(L, 2, 0, 0) != LUA_OK )
  {
    printf("ERROR : in 'write_data()'  '%s'\n", lua_tostring(L, -1));
    return -1;
  }

  return 0;
}


/**
  * @brief PLI - обёртка для функции init_lua(const char *fname)
  */
PLI_INT32 calltf_init_lua(PLI_BYTE8 *user_data)
{
  const char *fname;
  vpiHandle inst_h;
  vpiHandle arg_iter;
  vpiHandle fname_hdl;
  vpiHandle descriptor_hdl;
  s_vpi_value value_s;
  lua_State* descriptor;


  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

  if(arg_iter == NULL)
  {
    printf("if(arg_iter == NULL)\n");
    return 0;
  }

  fname_hdl = vpi_scan(arg_iter);

  if(fname_hdl == NULL)
  {
    printf("if(fname_hdl) == NULL)\n");
    return 0;
  }

  value_s.format = vpiStringVal;
  vpi_get_value(fname_hdl, &value_s);
  fname = value_s.value.str;

  if(fname == NULL)
  {
    printf("if(fname == NULL)\n");
    return 0;
  }

  descriptor_hdl = vpi_scan(arg_iter);

  if(descriptor_hdl == NULL)
  {
    printf("if(descriptor_hdl) == NULL)\n");
    return 0;
  }


  descriptor = (lua_State*)init_lua(fname);

  if(descriptor == NULL)
  {
    printf("if(descriptor == NULL)\n");
    return 0;
  }


  value_s.format = vpiIntVal;
  value_s.value.integer = (PLI_INT32)descriptor;//FIXME очень нехорошо
  vpi_put_value(descriptor_hdl, &value_s, NULL, vpiNoDelay);


  vpi_free_object(fname_hdl);
  vpi_free_object(descriptor_hdl);

  vpi_free_object(arg_iter);
  vpi_free_object(inst_h);

  return 0;
}


/**
  * @brief PLI - обёртка для функции exchange_CAD(lua_State* L, int32_t *CMD_O, int32_t *ADR_O, int32_t *DAT_O, const int32_t *DAT_I, const int32_t *STATUS_I)
  */
PLI_INT32 calltf_exchange_CAD(PLI_BYTE8 *user_data) //Verilog -> lua
{
  const char *fname;
  vpiHandle inst_h;
  vpiHandle arg_iter;

  s_vpi_time   vpi_time_s;

  vpiHandle descriptor_hdl;
  vpiHandle CMD_O_hdl;
  vpiHandle ADR_O_hdl;
  vpiHandle DAT_O_hdl;
  vpiHandle DAT_I_hdl;
  vpiHandle STATUS_I_hdl;
  vpiHandle result_hdl;

  s_vpi_value value_s;

  lua_State* descriptor;
  int32_t CMD_O;
  int32_t ADR_O;
  int32_t DAT_O;
  int32_t DAT_I;
  int32_t STATUS_I;
  int32_t result;

  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

  if(arg_iter == NULL)
  {
    printf("if(arg_iter == NULL)\n");
    return 0;
  }

  descriptor_hdl = vpi_scan(arg_iter);

  if(descriptor_hdl == NULL)
  {
    printf("if(descriptor_hdl) == NULL)\n");
    return 0;
  }


  CMD_O_hdl = vpi_scan(arg_iter);

  if(CMD_O_hdl == NULL)
  {
    printf("if(CMD_O_hdl) == NULL)\n");
    return 0;
  }

  ADR_O_hdl = vpi_scan(arg_iter);

  if(ADR_O_hdl == NULL)
  {
    printf("if(ADR_O_hdl) == NULL)\n");
    return 0;
  }

  DAT_O_hdl = vpi_scan(arg_iter);

  if(DAT_O_hdl == NULL)
  {
    printf("if(DAT_O_hdl) == NULL)\n");
    return 0;
  }

  DAT_I_hdl = vpi_scan(arg_iter);

  if(DAT_I_hdl == NULL)
  {
    printf("if(DAT_I_hdl) == NULL)\n");
    return 0;
  }

  STATUS_I_hdl = vpi_scan(arg_iter);

  if(STATUS_I_hdl == NULL)
  {
    printf("if(STATUS_I_hdl) == NULL)\n");
    return 0;
  }

  result_hdl = vpi_scan(arg_iter);

  if(result_hdl == NULL)
  {
    printf("if(result_hdl) == NULL)\n");
    return 0;
  }


  value_s.format = vpiIntVal;
  vpi_get_value(descriptor_hdl, &value_s);
  descriptor = (lua_State*)value_s.value.integer;

  vpi_get_value(DAT_I_hdl, &value_s);
  DAT_I = value_s.value.integer;

  vpi_get_value(STATUS_I_hdl, &value_s);
  STATUS_I = value_s.value.integer;

  result = exchange_CAD(descriptor, &CMD_O, &ADR_O, &DAT_O, &DAT_I, &STATUS_I);



  value_s.value.integer = CMD_O;
  vpi_put_value(CMD_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = ADR_O;
  vpi_put_value(ADR_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = DAT_O;
  vpi_put_value(DAT_O_hdl, &value_s, NULL, vpiNoDelay);

  value_s.value.integer = result;
  vpi_put_value(result_hdl, &value_s, NULL, vpiNoDelay);


  vpi_free_object(descriptor_hdl);
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


/**
  * @brief PLI - обёртка для функции read_data(lua_State* L, const int32_t *CMD_I, int32_t *DAT_O)
  */
PLI_INT32 calltf_read_data(PLI_BYTE8 *user_data) //Verilog -> lua
{
  const char *fname;
  vpiHandle inst_h;
  vpiHandle arg_iter;

  s_vpi_time   vpi_time_s;

  vpiHandle descriptor_hdl;
  vpiHandle CMD_I_hdl;
  vpiHandle DAT_O_hdl;


  s_vpi_value value_s;

  lua_State* descriptor;
  int32_t CMD_I;
  int32_t DAT_O;
  int32_t result;

  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

  if(arg_iter == NULL)
  {
    printf("if(arg_iter == NULL)\n");
    return 0;
  }

  descriptor_hdl = vpi_scan(arg_iter);

  if(descriptor_hdl == NULL)
  {
    printf("if(descriptor_hdl) == NULL)\n");
    return 0;
  }


  CMD_I_hdl = vpi_scan(arg_iter);

  if(CMD_I_hdl == NULL)
  {
    printf("if(CMD_I_hdl) == NULL)\n");
    return 0;
  }


  DAT_O_hdl = vpi_scan(arg_iter);

  if(DAT_O_hdl == NULL)
  {
    printf("if(DAT_O_hdl) == NULL)\n");
    return 0;
  }


  value_s.format = vpiIntVal;
  vpi_get_value(descriptor_hdl, &value_s);
  descriptor = (lua_State*)value_s.value.integer;

  vpi_get_value(CMD_I_hdl, &value_s);
  CMD_I = value_s.value.integer;

  result = read_data(descriptor, &CMD_I, &DAT_O);

  value_s.value.integer = DAT_O;
  vpi_put_value(DAT_O_hdl, &value_s, NULL, vpiNoDelay);


  vpi_free_object(descriptor_hdl);
  vpi_free_object(CMD_I_hdl);
  vpi_free_object(DAT_O_hdl);

  vpi_free_object(arg_iter);
  vpi_free_object(inst_h);

  return 0;
}


/**
  * @brief PLI - обёртка для функции write_data(lua_State* L, const int32_t *DAT_I)
  */
PLI_INT32 calltf_write_data(PLI_BYTE8 *user_data) //Verilog -> lua
{
  const char *fname;
  vpiHandle inst_h;
  vpiHandle arg_iter;

  s_vpi_time   vpi_time_s;

  vpiHandle descriptor_hdl;
  vpiHandle TIME_I_hdl;
  vpiHandle DAT_I_hdl;

  s_vpi_value value_s;

  lua_State* descriptor;
  int32_t TIME_I;
  int32_t DAT_I;
  int32_t result;

  inst_h = vpi_handle(vpiSysTfCall, NULL);
  arg_iter = vpi_iterate(vpiArgument, inst_h);

  if(arg_iter == NULL)
  {
    printf("if(arg_iter == NULL)\n");
    return 0;
  }

  descriptor_hdl = vpi_scan(arg_iter);

  if(descriptor_hdl == NULL)
  {
    printf("if(descriptor_hdl) == NULL)\n");
    return 0;
  }


  TIME_I_hdl = vpi_scan(arg_iter);

  if(TIME_I_hdl == NULL)
  {
    printf("if(TIME_I_hdl) == NULL)\n");
    return 0;
  }


  DAT_I_hdl = vpi_scan(arg_iter);

  if(DAT_I_hdl == NULL)
  {
    printf("if(DAT_I_hdl) == NULL)\n");
    return 0;
  }


  value_s.format = vpiIntVal;
  vpi_get_value(descriptor_hdl, &value_s);
  descriptor = (lua_State*)value_s.value.integer;

  vpi_get_value(TIME_I_hdl, &value_s);
  TIME_I = value_s.value.integer;

  vpi_get_value(DAT_I_hdl, &value_s);
  DAT_I = value_s.value.integer;


  result = write_data(descriptor, &TIME_I, &DAT_I);


  vpi_free_object(descriptor_hdl);
  vpi_free_object(TIME_I_hdl);
  vpi_free_object(DAT_I_hdl);


  vpi_free_object(arg_iter);
  vpi_free_object(inst_h);

  return 0;
}


int adderSizetf(char* user_data)
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
  systf_data.tfname = "$init_Lua";
  systf_data.calltf = calltf_init_lua;
  systf_data.compiletf = 0;
  systf_data.sizetf = 0;
  systf_data.user_data = 0;
  systf_handle = vpi_register_systf(&systf_data);
  vpi_free_object(systf_handle);

  systf_data.type = vpiSysTask;
  systf_data.sysfunctype = 0;
  systf_data.tfname = "$exchange_CAD";
  systf_data.calltf = calltf_exchange_CAD;
  systf_data.compiletf = 0;
  systf_data.sizetf = 0;
  systf_data.user_data = 0;
  systf_handle = vpi_register_systf(&systf_data);
  vpi_free_object(systf_handle);

  systf_data.type = vpiSysTask;
  systf_data.sysfunctype = 0;
  systf_data.tfname = "$read_data";
  systf_data.calltf = calltf_read_data;
  systf_data.compiletf = 0;
  systf_data.sizetf = 0;
  systf_data.user_data = 0;
  systf_handle = vpi_register_systf(&systf_data);
  vpi_free_object(systf_handle);
  
  systf_data.type = vpiSysTask;
  systf_data.sysfunctype = 0;
  systf_data.tfname = "$write_data";
  systf_data.calltf = calltf_write_data;
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
  tf_data.tfname      = "$init_Lua";
  tf_data.calltf      = calltf_init_lua;
  tf_data.compiletf   = NULL;
  tf_data.sizetf      = adderSizetf;
  vpi_register_systf(&tf_data);

  tf_data.type        = vpiSysFunc;
  tf_data.sysfunctype = vpiSysFuncSized;
  tf_data.tfname      = "$exchange_CAD";
  tf_data.calltf      = calltf_exchange_CAD;
  tf_data.compiletf   = NULL;
  tf_data.sizetf      = adderSizetf;
  vpi_register_systf(&tf_data);
  
  tf_data.type        = vpiSysFunc;
  tf_data.sysfunctype = vpiSysFuncSized;
  tf_data.tfname      = "$read_data";
  tf_data.calltf      = calltf_read_data;
  tf_data.compiletf   = NULL;
  tf_data.sizetf      = adderSizetf;
  vpi_register_systf(&tf_data);  
  
  tf_data.type        = vpiSysFunc;
  tf_data.sysfunctype = vpiSysFuncSized;
  tf_data.tfname      = "$write_data";
  tf_data.calltf      = calltf_write_data;
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