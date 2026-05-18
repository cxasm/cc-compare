#include "noequallinemanager.h"
#include "blockuserdata.h"
#include "qscidisplaywindow.h"
#include "rcglobal.h"

#include <QCoreApplication>

NoEqualLineManager::NoEqualLineManager(QList<NoEqualBlock>* left, QList<NoEqualBlock>* right, QsciDisplayWindow* leftSrc, QsciDisplayWindow* rightSrc, \
	QList<BlockUserData*>* leftExternBlockInfo, QList<BlockUserData*>* rightExternBlockInfo): \
	m_leftNoEqualBlocks(left), m_rightNoEqualBlocks(right), m_leftSrc(leftSrc), m_rightSrc(rightSrc),m_leftExternBlockInfo(leftExternBlockInfo), m_rightExternBlockInfo(rightExternBlockInfo)
{
}

NoEqualLineManager::~NoEqualLineManager()
{
}


//dire 方向 从start开始，删除掉deleteNum行。这个函数的逻辑不对，20210714，还需要修改，问题一大堆，好好把思路整理清楚后，再动笔
//20210715
void NoEqualLineManager::deleteLine(int dire, int start, int deleteNums)
{
	QList<NoEqualBlock>* noEqualBlocks = (dire == 0) ? m_leftNoEqualBlocks : m_rightNoEqualBlocks;

	//QsciDisplayWindow* displaySrc = (dire == 0) ? m_leftSrc : m_rightSrc;

	//QList<BlockUserData*>* externBlockInfo = (dire == 0) ? m_leftExternBlockInfo : m_rightExternBlockInfo;

	//区间包含[int line1Start, int line1End] 与 [line2Start, int line2End]的交集
	auto intersection = [](int line1Start, int line1End, int line2Start, int line2End, int& begin, int& end)->bool
	{
		if ((line1End < line2Start) || (line1Start > line2End))
		{
			return false;
		}

		if (line2Start >= line1Start && line1End <= line2End)
		{
			begin = line2Start;
			end = line1End;
		}
		else if (line2Start >= line1Start && line1End > line2End)
		{
			begin = line2Start;
			end = line2End;
		}
		else if (line1Start >= line2Start && line1End <= line2End)
		{
			begin = line1Start;
			end = line1End;
		}
		else if (line1Start >= line2Start && line1End > line2End)
		{
			begin = line1Start;
			end = line2End;
		}
		else
		{
			return false;
		}

		return true;
	};

	int interBegin = 0;
	int interEnd = 0;

	int lastModifyLineNum = start + deleteNums - 1;

	//尾部往前遍历，会删除
	for (int index = noEqualBlocks->size()-1;index >=0; --index)
	{
		NoEqualBlock &info = (*noEqualBlocks)[index];

		//前面不会再有交集了
		if (info.startBlockNums+info.blockLens-1 < start)
		{
			break;
		}

		//在lastModifyLineNum行之后的区间，则起始直接减去deleteNums
		//注意这里已经是在做不等块起始行号的调整了
		if (info.startBlockNums > lastModifyLineNum)
		{
			info.startBlockNums -= deleteNums;
			continue;
		}

		//剩下的就是在start到lastModifyLineNum之间的，一定有交集区域
		bool ok = intersection(info.startBlockNums, info.startBlockNums + info.blockLens - 1, start, start + deleteNums - 1, interBegin, interEnd);
		if (ok)
		{
			int leftInter = interBegin - info.startBlockNums;
			int rightInter = info.startBlockNums + info.blockLens - 1 - interEnd;

			assert(leftInter >= 0);
			assert(rightInter >= 0);

			//全部删除掉了
			if (leftInter == 0 && rightInter == 0)
			{
				noEqualBlocks->removeAt(index);
			}
			else if (leftInter > 0 && rightInter > 0)
			{
				//20210808 发现问题，这里其实把一个大的不等区域，从中间开始分裂为2个
				//所以这里必须要处理分裂为2个区域的情况。之前没有考虑，导致一个不等大区域中修改时
				//导致错误的情况。

				//20210808 将区域分裂为2段，注意外面遍历是逆序开始，这里添加元素，不会造成迭代器的遍历稳定性破坏

				info.blockLens = leftInter;

				//添加一个下一段。目前在操作index序号的元素，在其后面加一个元素，不会破坏迭代器
				NoEqualBlock addBlock(info.startBlockNums + leftInter, rightInter);
				noEqualBlocks->insert(index+1,addBlock);

				//这里其实也有下面的问题，需要增加一个marker的按钮
				//没有增加也是因为实际内容已经改变，这里用的还是改变前的行号，所以添加不会准确，需要延时到后面去添加
			}
			else if (leftInter > 0 && rightInter == 0)
			{
				info.blockLens = leftInter;
			}
			else if (leftInter == 0 && rightInter > 0)
			{
				info.startBlockNums = info.startBlockNums + info.blockLens - rightInter - deleteNums;
				info.blockLens = rightInter;

				//20210718这里不能添加，因为删除和添加都是在已经改变后再做的，并没有再删除时就做删除
				//如果这里加了，则加的是没有添加之前时的行号，但是实际已经添加过了，会导致加的并不是我们想要的地方。
				//怎么办呢，再后面添加的地方，统一来一次添加
				//20210808 上面思路是对的。但是发现一个bug，如果后续结果是相等的，则不会添加一个不等块。
				//则后续就没有机会去设置marker的信息了。需要对是相等块的情况做一个考虑处理
				/*displaySrc->markerAdd(info.startBlockNums, MARGIN_SYNC_BT);
				QCoreApplication::processEvents();
				externBlockInfo->at(info.startBlockNums)->m_MarkerFlag = MARGIN_SYNC_BT;*/
			}
			
		}
			
	}

}

//插入noEqualBlocks。注意，使用该函数的前提：插入的块noEqualBlocks与现有块不可能有重叠，提前已经在外面调整好了
void NoEqualLineManager::insertLine(int dire, QList<NoEqualBlock>& noEqualBlocks)
{
	if (noEqualBlocks.isEmpty())
	{
		return;
	}

	QList<NoEqualBlock>* blocks = ((dire == 0) ? m_leftNoEqualBlocks : m_rightNoEqualBlocks);
	QsciDisplayWindow* displaySrc = (dire == 0) ? m_leftSrc : m_rightSrc;
	QList<BlockUserData*>* externBlockInfo = (dire == 0) ? m_leftExternBlockInfo : m_rightExternBlockInfo;

	NoEqualBlock& first = noEqualBlocks.first();

	int index = blocks->size();

	for (auto it = blocks->rbegin(); it != blocks->rend(); ++it,--index)
	{
		if (first.startBlockNums > it->startBlockNums)
		{
			break;
		}
	}

	for (int i = 0; i < noEqualBlocks.size(); ++i)
	{
		const NoEqualBlock & v = noEqualBlocks.at(i);

		blocks->insert(index + i, v);

		displaySrc->markerAdd(v.startBlockNums, MARGIN_SYNC_BT);
		externBlockInfo->at(v.startBlockNums)->m_MarkerFlag = MARGIN_SYNC_BT;
		QCoreApplication::processEvents();
		for (int j = 1; j < v.blockLens; ++j)
		{
			displaySrc->markerAdd(v.startBlockNums+j, MARGIN_VER_LINE);
			QCoreApplication::processEvents();
			externBlockInfo->at(v.startBlockNums+j)->m_MarkerFlag = MARGIN_VER_LINE;
		}
	}
}

//从startLine开始,如果后面的行的开始值大于startLine，则调整后续偏移量
void NoEqualLineManager::adjustStartBlockNumAfterLineNum(int dire, int startLine, int offsetLines)
{
	QList<NoEqualBlock>* noEqualBlocks = (dire == 0) ? m_leftNoEqualBlocks : m_rightNoEqualBlocks;
	QList<BlockUserData*>* externBlockInfo = (dire == 0) ? m_leftExternBlockInfo : m_rightExternBlockInfo;
	QsciDisplayWindow* displaySrc = (dire == 0) ? m_leftSrc : m_rightSrc;

	for (int i = 0, lens = noEqualBlocks->size(); i < lens; ++i)
	{
		NoEqualBlock& info = (*noEqualBlocks)[i];
		if (info.startBlockNums >= startLine)
		{
			info.startBlockNums += offsetLines;


			//20210718上面leftInter == 0 && rightInter > 0的地方，没有增加MARGIN_SYNC_BT，则这里添加一下
			BlockUserData* t = externBlockInfo->at(info.startBlockNums);
			if (t->m_MarkerFlag != MARGIN_SYNC_BT)
			{
				if (t->m_MarkerFlag == MARGIN_VER_LINE)
				{
					displaySrc->markerDelete(info.startBlockNums, MARGIN_VER_LINE);
				}
				displaySrc->markerAdd(info.startBlockNums, MARGIN_SYNC_BT);
				//QCoreApplication::processEvents();
				t->m_MarkerFlag = MARGIN_SYNC_BT;
			}
			
		}
	}
}
