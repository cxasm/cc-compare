#pragma once

//专门用来管理不等块操作的类

#include <QList>
#include "rcglobal.h"

class BlockUserData;
class QsciDisplayWindow;

class NoEqualLineManager
{
public:
	NoEqualLineManager(QList<NoEqualBlock>* left, QList<NoEqualBlock>* right, QsciDisplayWindow* leftSrc, QsciDisplayWindow* rightSrc, QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo);
	~NoEqualLineManager();

	void deleteLine(int dire, int start, int deleteNums);

	void insertLine(int dire, QList<NoEqualBlock>& noEqualBlocks);
	void adjustStartBlockNumAfterLineNum(int dire, int startLine, int offsetLines);
private:
	/*void adjustStartBlockNum(int dire, int startIndex, int offsetLines);*/

	

private:
	NoEqualLineManager(const NoEqualLineManager& other) = delete;
	NoEqualLineManager& operator=(const NoEqualLineManager& other) = delete;

private:
	QList<NoEqualBlock>* m_leftNoEqualBlocks;
	QList<NoEqualBlock>* m_rightNoEqualBlocks;

	QsciDisplayWindow* m_leftSrc;
	QsciDisplayWindow* m_rightSrc;

	QList<BlockUserData*>* m_leftExternBlockInfo;
	QList<BlockUserData*>* m_rightExternBlockInfo;
};

