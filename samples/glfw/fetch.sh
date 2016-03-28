#!/bin/bash

rm -rf data glfw-3.1.zip

wget http://sourceforge.net/projects/glfw/files/glfw/3.1/glfw-3.1.zip/download -O glfw-3.1.zip
dtrx glfw-3.1.zip
mv glfw-3.1 data
rm glfw-3.1.zip
