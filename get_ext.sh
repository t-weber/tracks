#!/bin/bash
#
# get external dependencies
# @author Tobias Weber (orcid: 0000-0002-7230-1932)
# @date nov-2024
# @license see 'LICENSE' file
#

mkdir -pv ext
pushd ext

echo -e "\n--------------------------------------------------------------------------------"
echo -e "Getting QCustomPlot..."
echo -e "--------------------------------------------------------------------------------"
wget https://www.qcustomplot.com/release/2.1.1/QCustomPlot-source.tar.gz
tar -xzvf QCustomPlot-source.tar.gz \
	qcustomplot-source/qcustomplot.cpp \
	qcustomplot-source/qcustomplot.h
mv -v qcustomplot-source/* .
rmdir -v qcustomplot-source
echo -e "--------------------------------------------------------------------------------\n"

popd
