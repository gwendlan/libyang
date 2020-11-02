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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include \"common.h\"
#include \"context.h\"
#include \"dict.h\"
#include \"log.h\"
#include \"parser_data.h\"
#include \"plugins_types.h\"
#include \"printer_internal.h\"
#include \"tree.h\"
#include \"tree_schema.h\"
#include \"tree_schema_internal.h\"
#include \"xpath.h\"
#include \"out_internal.h\"

" > printer_tree.c

sed -n '/Declarations start/,/Declarations end/ p ' new.h >> printer_tree.c
sed -n '/Definitions start/,/Definitions end/ p ' new.c >> printer_tree.c
