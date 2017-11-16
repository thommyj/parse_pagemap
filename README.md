# parse_pagemap
Parse /proc/&lt;pid>/map, corresponding pagemap, kpagemap and kpagecnt  and list all mappings

usage parse_pagemap  [--combine] <pid>
when specifying combine pages with the same mapping and attributes will be combined to one row

compile with
gcc parse_pagemap.c -o parse_pagemap

tested on linux 4.4 x86 and linux 4.9.0 arm64
example output
./parse_pagemap 719 | head

vaddr size pfn type exclusive flags mappings name
0x0000000000400000 0x0000000000001000 0x00000000000098ed r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000401000 0x0000000000001000 0x00000000000098ee r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000402000 0x0000000000001000 0x00000000000098ef r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000403000 0x0000000000001000 0x00000000000098f0 r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000404000 0x0000000000001000 0x00000000000098f1 r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000405000 0x0000000000001000 0x00000000000098f2 r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000406000 0x0000000000001000 0x00000000000098f3 r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000407000 0x0000000000001000 0x00000000000098f4 r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
0x0000000000408000 0x0000000000001000 0x00000000000098f5 r-xp 0 0x000000400000487c 3 /usr/sbin/dropbearmulti
..
..
