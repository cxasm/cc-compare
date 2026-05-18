#include "StrategyCompare.h"
#include "LcsTemplate.h"

//对比策略类。主要解决不相等的块，如何进行对齐的问题。
//退出beycome的测试：曲率容差，左右文字前后错一个偏移距，进行一个对比最大值
//就按照最大值来决定对齐

//(1)好像只找到第一行的最大匹配，而后面的都根据第一行之间决定，这样就简化了大量问题
//(2)不完全是只匹配第一行，先找到第一行的最大匹配行。然后再接着找到第二行的最大匹配行。
//(3)如果有空白行这种完全相等的行，则需要分块。将相等行的上下重新分为2个新块

StrategyCompare::StrategyCompare()
{
}


StrategyCompare::~StrategyCompare()
{
}


//参数：块信息，文件指针，行信息
//一对一的对比，不进行对齐处理。

void StrategyCompare::BlockCmpLcs(const BlocksInfo & leftBlockInfo, uchar* leftFileData, QVector<LineFileInfo> &leftLinesInfo, \
	const BlocksInfo & rightBlockInfo, uchar* rightFileData, QVector<LineFileInfo> &rightLinesInfo, \
	BlockCmpPairResult &result)
{
	int leftNums = leftBlockInfo.actualNums;
	int rightNums = rightBlockInfo.actualNums;

	if (leftNums == 1 && rightNums == 1)
	{
		//如果均只有1行，则不用偏差。
		result.direction = 2;
		return;
	}

	//用行少的去匹配行多的

	if (leftNums >= rightNums)
	{
		result.direction = 0;
		lessCmpMore(rightBlockInfo, rightFileData, rightLinesInfo, leftBlockInfo, leftFileData, leftLinesInfo, result.data);
	}
	else
	{
		result.direction = 1;
		lessCmpMore(leftBlockInfo, leftFileData, leftLinesInfo, rightBlockInfo, rightFileData, rightLinesInfo, result.data);
	}
}




//我们假设左边是多行块，右边是少行块。这个函数对比少到多，找出右边每行对应左边的行，这两行对应的行一定是公共子串最大的关系
//但是有一点，不是每行都可能找到匹配。，找不到行，要算做是对比块中新增的那些行
void StrategyCompare::lessCmpMore(const BlocksInfo & lessBlockInfo, uchar* /*lessFileData*/, QVector<LineFileInfo> &lessLinesInfo, \
	const BlocksInfo & moreBlockInfo, uchar* /*moreFileData*/, QVector<LineFileInfo> &moreLinesInfo,QMap<int, BlockCmpPairInfo>& result)
{

	//第二层匹配是一直往后的，不是每次从头
	int moreStartIndex = moreBlockInfo.startLine;

	//外面一层是行少的。循环结束时，可能有的行没有匹配的行，那么认定那些行是相对新增的行
	for (int i = lessBlockInfo.startLine; i < lessBlockInfo.endLine; ++i)
	{
		int maxValue = 0;

		//取出少行块对应的一行内容
		QChar *startLess = (QChar *)lessLinesInfo.at(i).unicodeStr.data();
		int lessLinesLength = lessLinesInfo.at(i).unicodeStr.count();

		for (int j = moreStartIndex; j < moreBlockInfo.endLine; ++j)
		{
			//依次取出多行块对应的一行内容
			QChar * startMore = (QChar *)moreLinesInfo.at(j).unicodeStr.data();
			int moreLinesLength = moreLinesInfo.at(j).unicodeStr.count();

			//做一个最大公共子串的算法
			int value = LcsTemplate<QChar>::getLcsLength(lessLinesLength, moreLinesLength, startLess, startMore);
			if (value > maxValue)
			{
				maxValue = value;

				//下次就要从后面继续开始了
				moreStartIndex = j + 1;

				//如果左右已经完全匹配了,则需要再分段，这种情况只有完全是空白行的情况会出现了
				if (value == moreLinesLength && value == lessLinesLength)
				{
					//需要再分配。
					result.insert(i,BlockCmpPairInfo(1, i, j));
					break;
				}
				else
				{
					//不是完全匹配的，但是最大匹配值,更新
					result.insert(i,BlockCmpPairInfo(2, i, j));
				}
			}
		}
		//如果没有找到匹配项，可以认定为类似新增，按照多出一行的个数进行
		if (0 == maxValue)
		{
			result.insert(i, BlockCmpPairInfo(3,i,-1));
		}
	}
}
