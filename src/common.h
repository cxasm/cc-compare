#pragma once
#include <Qt> 

const int ResultItemPos = Qt::UserRole + 1;//每一条查找结果，section下面的子级别
const int ResultItemEditor = Qt::UserRole + 2;//查找结果的一个总节点的属性字段，section级别
const int ResultWhatFind = Qt::UserRole + 3;
const int ResultItemRoot = Qt::UserRole + 4; //一次查找结果的根节点的属性字段，多个section的父级别
const int ResultItemEditorFilePath = Qt::UserRole + 5;
const int ResultItemLen = Qt::UserRole + 6;
const int ResultItemDesc = Qt::UserRole + 7;

int nbDigitsFromNbLines(size_t nbLines);

