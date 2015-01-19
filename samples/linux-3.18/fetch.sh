#!/bin/bash

mkdir data

wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.18.tar.xz
dtrx linux-3.18.tar.xz
mv linux-3.18  data

wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.18.1.tar.xz
dtrx linux-3.18.1.tar.xz
mv linux-3.18.1 data

wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.18.2.tar.xz
dtrx linux-3.18.2.tar.xz
mv linux-3.18.2 data

wget https://www.kernel.org/pub/linux/kernel/v3.x/linux-3.18.3.tar.xz
dtrx linux-3.18.3.tar.xz
mv linux-3.18.3 data

rm *.tar.xz
