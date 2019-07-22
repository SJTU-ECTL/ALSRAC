#!/usr/bin/env python
# -*- coding: utf-8 -*-

for a in range(2):
    for b in range(2):
        for c in range(2):
            for d in range(2):
                f = not(a and
                        not(a and not(b)) and
                        not(a and b and c) and
                        not(a and b and not(c) and d)
                        )
                print(a, b, c, d, f)
