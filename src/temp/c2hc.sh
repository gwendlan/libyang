#! /bin/sh

echo "
/**
 * @file printer_tree.c
 * @author Adam Piecek <piecek@cesnet.cz>
 * @brief RFC tree printer for libyang data structure
 *
 * Copyright (c) 2015 - 2020 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the \"License\").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#ifndef NEW_H_
#define NEW_H_

#include <stdint.h> /* uint_, int_ */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h> /* NULL */

#define LY_ERR int
#define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
typedef uint8_t ly_bool;
struct ly_out;
struct lys_module;
struct lysp_submodule;
struct lysc_node;

" > new.h

sed -n '/Declarations start/,/Declarations end/ p ' printer_tree.c >> new.h
echo "#endif" >> new.h

echo "
#include \"new.h\"
#include <string.h> // strlen
" > new.c
sed -n '/Definitions start/,/Definitions end/ p ' printer_tree.c >> new.c

