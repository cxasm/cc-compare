#include "BlockCompare.h"
#include "Lcs.h"
#include "LcsTemplate.h"



BlockCompare::BlockCompare():AbstractCompare()
{

}

BlockCompare::~BlockCompare()
{

}
//2020-4-11发现问题：如果\r\n是分开2个小段，则最后显示的时候会分两行，这是比较怪的
//所以对这个情况进行检查，强行将\r\n合并为一个section
//20230404 发现一个新问题，最后的换行本来是相等，但是归并到上一节后，其是否相等，取决于上一节。
//这在空白显示下，是比较奇怪的，用户可能觉得对换行符号进行对比发生了错误。
void BlockCompare::dealLineTail(QVector<SectionNode> & result)
{
	//将最后的换行段归并到前一个
	if ((result.size() >= 2) && ((result.last().text.compare("\r\n") == 0) || (result.last().text.compare("\n")==0) || \
		(result.last().text.compare("\r") == 0)))
	{
		SectionNode last = result.takeLast();
		SectionNode& curLast = result.last();
		curLast.text.append(last.text);
		//如果前后节相等状态不一样
		if (last.equal != curLast.equal)
		{
			curLast.tailStatus = (last.equal ? 1 : 2);

			//后面在设置相等或不等时，要扣除尾巴上面的长度。因为我们强行把尾巴附加到了前一个节。
			curLast.tailLens = last.text.size();
	}
}
}

//处理落单的相等的非ASCII字符。可能是中文字符的半个字符，需要视作不等处理
void BlockCompare::addSectionNode(QVector<SectionNode> & result, SectionNode &node)
{
	result.append(node);
}

#ifdef CMP_CODE_NOEQUAL

void BlockCompare::lineCmpLcs(const int lineNum, QChar* lineSrc, QChar* lcsSrc, int lcsLens, \
	QVector<SectionNode> & result)
{
	if (lcsLens <= 0 || lcsSrc == nullptr)
	{
		return;
	}

	int fileSrcIndex = 0;
	int noEqualTimes = 0;
	int equalTimes = 0;
	int lcsSrcIndex = 0;


	QChar* start = lineSrc;
	int linesLength = lineNum;

	//第二层循环，处理一行的每一个字符
	for (fileSrcIndex = 0; fileSrcIndex < linesLength;)
	{
		//第三层循环，对比每一个字符与lcs相等的关系。并把相等的和不相等的分开为一段段的SectionNode，加入容器
		while ((fileSrcIndex < linesLength) && (start[fileSrcIndex] != lcsSrc[lcsSrcIndex]))
		{
			if (equalTimes > 0)
			{
				SectionNode cmpinfo;
				cmpinfo.equal = true;

				QString utf16Str(start + fileSrcIndex - equalTimes, equalTimes);
				cmpinfo.text.append(utf16Str);

				addSectionNode(result, cmpinfo);
				equalTimes = 0;
			}
			++fileSrcIndex;
			++noEqualTimes;
		}

		//加入不相等的
		if (noEqualTimes > 0)
		{
			SectionNode cmpinfo;
			cmpinfo.equal = false;

			QString utf16Str(start + fileSrcIndex - noEqualTimes, noEqualTimes);
			cmpinfo.text.append(utf16Str);
	
			addSectionNode(result, cmpinfo);
			noEqualTimes = 0;
		}
		//遇到一个相等的了
		if (fileSrcIndex < linesLength)
		{
			++equalTimes;
			++lcsSrcIndex;
			++fileSrcIndex;
		}
	}

	//退出第二层循环时，处理尾部的情况
	if (equalTimes > 0) 
	{
		SectionNode cmpinfo;
		cmpinfo.equal = true;
		QString utf16Str(start + fileSrcIndex - equalTimes, equalTimes);
		cmpinfo.text.append(utf16Str);

		addSectionNode(result, cmpinfo);
		equalTimes = 0;
	}
	else if (noEqualTimes > 0)
	{
		SectionNode cmpinfo;
		cmpinfo.equal = false;
		QString utf16Str(start + fileSrcIndex - noEqualTimes, noEqualTimes);
		cmpinfo.text.append(utf16Str);

		addSectionNode(result, cmpinfo);
		noEqualTimes = 0;
	}

	//合并尾部后面的\r\n为一个section。由于\r\n是无法显示出来的，所以可以不关心是等于还是非等于
	dealLineTail(result);

}

void BlockCompare::fillCmpLineResutl(bool status, const QString& leftLineContent, const QString& rightLineContent, LineNode &outputLeft, LineNode &outputRight)
{
	SectionNode left;
	left.equal = status;

	left.text = leftLineContent;
	outputLeft.lineText.append(left);

	SectionNode right;
	right.equal = status;

	right.text = rightLineContent;
	outputRight.lineText.append(right);
}

//对比一行的情况,将左右行进行lcs对比，把相等和不等的子串保存到output中
//返回值：是否完全相等。是true 否false 。equalRatio:返回相等的相似律
bool BlockCompare::cmpLine(int leftLineNum, int rightLineNum, QList<LineFileInfo>& leftLinesInfo, QList<LineFileInfo>& rightLinesInfo, LineNode &outputLeft, LineNode &outputRight)
{

	bool isBothEmptyLine = (leftLinesInfo.at(leftLineNum).isEmptyLine && rightLinesInfo.at(rightLineNum).isEmptyLine);
	const LineFileInfo& leftLineData = leftLinesInfo.at(leftLineNum);
	const LineFileInfo& rightLineData = rightLinesInfo.at(rightLineNum);

		
	if (isBothEmptyLine || ((leftLineData.unicodeStr.size() <= 2) && (rightLineData.unicodeStr.size() <= 2) && (leftLineData.unicodeStr == rightLineData.unicodeStr)))
	{
		fillCmpLineResutl(true, leftLineData.unicodeStr, rightLineData.unicodeStr, outputLeft, outputRight);
		return true;
	}
	else if (leftLineData.isEmptyLine || rightLineData.isEmptyLine)
	{
		//20211014 发现bug:当leftLen=0，右边是\r\n是，对比发现lcsLength = 0，下面进行lcs.cmp时进入到了死循环中出不来了
		//故进行lcs.cmp的前提是两个字符串都必须要有内容
		//有一个是空，也不需要比较，这是明显的
		fillCmpLineResutl(false, leftLineData.unicodeStr, rightLineData.unicodeStr, outputLeft, outputRight);
		return false;
	}

	//下面才是两个行都有内容的情况

	QChar *leftQChars = (QChar*)leftLineData.unicodeStr.data();
	int leftLen = leftLineData.unicodeStr.size();

	
	QChar *rightQChars = (QChar*)rightLineData.unicodeStr.data();
	int rightLen = rightLineData.unicodeStr.size();

	//进行基于UTF16的对比
	//只有一行
	int lcsLength = LcsTemplate<QChar>::getLcsLength(leftLen, rightLen, leftQChars, rightQChars);

	//完全相等，这种就出现在都是空行的情况。否则的话，完全相等的行早就被识别出来过了
	if ((lcsLength == leftLen) && (lcsLength == rightLen))
	{

		SectionNode left;
		left.equal = true;
		left.text = QString(leftQChars);
		outputLeft.lineText.append(left);

		SectionNode right;
		right.equal = true;
		right.text = QString(rightQChars);
		outputRight.lineText.append(right);

		return true;
	}
	else
	{
		if (lcsLength == 0)
		{
			//对于完全没有lcs的情况，也不用进行对比为好
			fillCmpLineResutl(false, leftLineData.unicodeStr, rightLineData.unicodeStr, outputLeft, outputRight);

			return false;
		}

		//最后才是有lcs的情况

		int retLenght = 0;
		QChar * lcsLine = nullptr;

		LcsTemplate<QChar> lcs(leftQChars, rightQChars, leftLen, rightLen);
		lcsLine = lcs.cmp(retLenght);


		if (retLenght > 0)
		{
			lineCmpLcs(leftLen, leftQChars, lcsLine, retLenght, outputLeft.lineText);
			lineCmpLcs(rightLen, rightQChars, lcsLine, retLenght, outputRight.lineText);
		}

		if (lcsLine != nullptr)
		{
			delete[]lcsLine;
			lcsLine = nullptr;
		}
	}
	return false;
}
#endif

//对比两个块。块-线-小段，这是三个容器逻辑概念。leftBlocks 是输出块
void BlockCompare::blockCmpLcs(const BlocksInfo & leftBlockInfo,  QList<LineFileInfo>& leftLinesInfo, \
	const BlocksInfo & rightBlockInfo, QList<LineFileInfo>& rightLinesInfo, \
	QVector<BlockNode> &leftBlocks, QVector<BlockNode> &rightBlocks)
{
	int leftNums = 0;
	int rightNums = 0;

	for (int i = leftBlockInfo.startLine; i < leftBlockInfo.endLine; ++i)
	{
		leftNums += leftLinesInfo.at(i).unicodeStr.count();
	}

	for (int i = rightBlockInfo.startLine; i < rightBlockInfo.endLine; ++i)
	{
		rightNums += rightLinesInfo.at(i).unicodeStr.count();
	}

	//两边都有内容要输出
	if (leftNums > 0 && rightNums > 0)
	{
		int leftLineNums = leftBlockInfo.actualNums;
		int rightLineNums = rightBlockInfo.actualNums;

		//左右行都只有1行
		if (leftLineNums == 1 && rightLineNums == 1)
		{
			//如果均只有1行，则不用偏差。
			int leftLineNum = leftBlockInfo.startLine;
			int rightLineNum = rightBlockInfo.startLine;

			//20220407进行了替换修改。1比1时，严格模式也可以认定为不相等，不一定就是一定认定为相等。
			//20220422又恢复了，发现大bug，会导致修改后同步严重失效
			createBlockWithOneLine(leftLineNum, rightLineNum, leftLinesInfo, rightLinesInfo, \
				leftBlocks, rightBlocks);
			
			//lessCmpMore(rightBlockInfo, rightLinesInfo, leftBlockInfo, leftLinesInfo, rightBlocks, leftBlocks);
	
		}
		else if (leftLineNums >= rightLineNums)
		{
			//用行少的去匹配行多的
			lessCmpMore(rightBlockInfo, rightLinesInfo, leftBlockInfo, leftLinesInfo, rightBlocks,leftBlocks);
		}
		else
		{
			lessCmpMore(leftBlockInfo, leftLinesInfo, rightBlockInfo, rightLinesInfo, leftBlocks, rightBlocks);
		}
	}
	else if (leftNums > 0) //只有左边有内容输出。只有最后一块
	{
		//右边是少 左边是多，
		BlocksInfo lessBlockInfo_(false, 0, 0, 0);
		BlocksInfo moreBlockInfo_(false, leftBlockInfo.startLine, leftBlockInfo.endLine, leftBlockInfo.endLine - leftBlockInfo.startLine);

		//先插入前面不相等的块
		createBlockWithMultiNoEqualLines(lessBlockInfo_,rightLinesInfo, \
			moreBlockInfo_, leftLinesInfo, \
			rightBlocks, leftBlocks);

	}
	else if (rightNums > 0) //只有右边有内容输出。只有最后一块
	{
		//左边是少 右边是多，
		BlocksInfo lessBlockInfo_(false, 0, 0, 0);
		BlocksInfo moreBlockInfo_(false, rightBlockInfo.startLine, rightBlockInfo.endLine, rightBlockInfo.endLine - rightBlockInfo.startLine);

		//先插入前面不相等的块
		createBlockWithMultiNoEqualLines(lessBlockInfo_, leftLinesInfo, \
			moreBlockInfo_, rightLinesInfo, \
			leftBlocks, rightBlocks);

	}
}

#ifdef CMP_CODE_NOEQUAL
//对比1行文本的情况
void BlockCompare::createBlockWithOneLine(int leftLineNum, int rightLineNum, QList<LineFileInfo>& leftLinesInfo, QList<LineFileInfo>& rightLinesInfo, \
	  QVector<BlockNode>& leftBlocks, QVector<BlockNode>& rightBlocks)
{
	//如果均只有1行，则不用偏差。
	LineNode outputLeft;
	outputLeft.lineNums = leftLineNum;

	LineNode outputRight;
	outputRight.lineNums = rightLineNum;

	bool isTotalEqual = cmpLine(leftLineNum, rightLineNum, leftLinesInfo, rightLinesInfo, outputLeft, outputRight);
	
	if (isTotalEqual)
	{
		//如果是完全相等。这种可能大概率是都是空白行的情况。要作为相等块进行输出
		outputLeft.totalEqual = true;
		outputRight.totalEqual = true;
	}

	BlockNode leftBlock;
	BlockNode rightBlock;

	leftBlock.type = 1;
	leftBlock.lineNodes.append(outputLeft);


	rightBlock.type = 1;
	rightBlock.lineNodes.append(outputRight);


	leftBlocks.append(leftBlock);
	rightBlocks.append(rightBlock);
}
#endif

//左右多行完全相等的行，归并为一个内部块
void BlockCompare::createBlockWithMultiEqualLines(const BlocksInfo & lessBlockInfo, QList<LineFileInfo> &lessLinesInfo, \
	const BlocksInfo & moreBlockInfo, QList<LineFileInfo> &moreLinesInfo, \
	QVector<BlockNode> &lessBlocks, QVector<BlockNode> &moreBlocks)
{
	BlockNode lessBlock;
	lessBlock.type = 1;

	BlockNode moreBlock;
	moreBlock.type = 1;

	//将少的块，作为新增块直接输出，而多的块填充对齐行
	for (int i = lessBlockInfo.startLine; i < lessBlockInfo.endLine; ++i)
	{
		LineNode oneLine;
		SectionNode oneSection;
		oneSection.equal = true;

	
		oneSection.text.append(lessLinesInfo[i].unicodeStr);
		oneLine.lineText.append(oneSection);

		oneLine.lineNums = i;
		oneLine.totalEqual = true; //行完全相等
		lessBlock.lineNodes.append(oneLine);
	}

	//再处理多边的块
	for (int i = moreBlockInfo.startLine; i < moreBlockInfo.endLine; ++i)
	{
		LineNode oneLine;
		SectionNode oneSection;
		oneSection.equal = true;

	
		oneSection.text.append(moreLinesInfo[i].unicodeStr);
		oneLine.lineText.append(oneSection);

		oneLine.lineNums = i;
		oneLine.totalEqual = true;
		moreBlock.lineNodes.append(oneLine);
	}

	lessBlocks.append(lessBlock);
	moreBlocks.append(moreBlock);

}

//左右多行，但是多行没有相等的，都看作是新增行
void BlockCompare::createBlockWithMultiNoEqualLines(const BlocksInfo & lessBlockInfo, QList<LineFileInfo> &lessLinesInfo, \
	const BlocksInfo & moreBlockInfo, QList<LineFileInfo> &moreLinesInfo, \
	QVector<BlockNode> &lessBlocks, QVector<BlockNode> &moreBlocks)
{
	BlockNode lessBlock;
	lessBlock.type = 1;

	BlockNode moreBlock;
	moreBlock.type = 2;
	moreBlock.alignRowCount = lessBlockInfo.endLine - lessBlockInfo.startLine;

	//将少的块，作为新增块直接输出，而多的块填充对齐行
	for (int i = lessBlockInfo.startLine; i < lessBlockInfo.endLine; ++i)
	{
		LineNode oneLine;
		SectionNode oneSection;
		oneSection.equal = false;


		oneSection.text.append(lessLinesInfo[i].unicodeStr);
		oneLine.lineText.append(oneSection);

		oneLine.lineNums = i;
		lessBlock.lineNodes.append(oneLine);
	}

	if (moreBlock.alignRowCount > 0)
	{
		lessBlocks.append(lessBlock);
		moreBlocks.append(moreBlock);
	}


	//再处理多边的块
	BlockNode moreBlock1;
	moreBlock1.type = 1;

	BlockNode lessBlock1;
	lessBlock1.type = 2;
	lessBlock1.alignRowCount = moreBlockInfo.endLine - moreBlockInfo.startLine;

	//将少的块，作为新增块直接输出，而多的块填充对齐行
	for (int i = moreBlockInfo.startLine; i < moreBlockInfo.endLine; ++i)
	{
		LineNode oneLine;
		SectionNode oneSection;
		oneSection.equal = false;

	
		oneSection.text.append(moreLinesInfo[i].unicodeStr);
		oneLine.lineText.append(oneSection);

		oneLine.lineNums = i;
		moreBlock1.lineNodes.append(oneLine);
	}

	if (lessBlock1.alignRowCount > 0)
	{
		lessBlocks.append(lessBlock1);
		moreBlocks.append(moreBlock1);
	}
}

//我们假设左边是多行块，右边是少行块。这个函数用少行块对比对多行块，找出右边每行对应左边的行，这两行对应的行一定是公共子串最大的关系
//但是有一点，不是每行都可能找到匹配。找不到行，要算做是对比块中新增的那些行
void BlockCompare::lessCmpMore(const BlocksInfo & lessBlockInfo, QList<LineFileInfo> &lessLinesInfo, \
	const BlocksInfo & moreBlockInfo, QList<LineFileInfo> &moreLinesInfo,\
	QVector<BlockNode> &lessBlocks, QVector<BlockNode> &moreBlocks)
{
	QMap<int, BlockCmpPairInfo> result;

	//第二层匹配是一直往后的，不是每次从头
	int moreStartIndex = moreBlockInfo.startLine;

	//外面一层是行少的。循环结束时，可能有的行没有匹配的行，那么认定那些行是相对新增的行
	for (int i = lessBlockInfo.startLine; i < lessBlockInfo.endLine; ++i)
	{

		if (m_isCancel != nullptr && *m_isCancel)
		{
			break;
		}

		float maxValue = 0.001f;

		//取出少行块对应的一行内容
		const LineFileInfo& lessfileInfo = lessLinesInfo.at(i);
		QChar *startLess = (QChar*)lessfileInfo.unicodeStr.data();
		int lessLinesLength = lessfileInfo.unicodeStr.count();

		//不比较最后的换行符
		lessLinesLength -= (lessfileInfo.lineEndFormat == RC_LINE_FORM::MAC_LINE) ? RC_LINE_FORM::UNIX_LINE : lessLinesInfo.at(i).lineEndFormat;
		if (lessLinesLength < 0)
		{
			continue;
		}

		for (int j = moreStartIndex; j < moreBlockInfo.endLine; ++j)
		{
			if (m_isCancel != nullptr && *m_isCancel)
			{
				break;
			}

			//依次取出多行块对应的一行内容
			const LineFileInfo& morefileInfo = moreLinesInfo.at(j);
			QChar *startMore = (QChar*)morefileInfo.unicodeStr.data();
			int moreLinesLength = morefileInfo.unicodeStr.count();

			//不比较最后的换行符
			moreLinesLength -= (morefileInfo.lineEndFormat == RC_LINE_FORM::MAC_LINE) ? RC_LINE_FORM::UNIX_LINE : morefileInfo.lineEndFormat;
			if (moreLinesLength < 0)
			{
				continue;
			}

			//如果都是空白行，则认定相等。给一个机会，让空白行可以匹配
			if (lessLinesLength == 0 && moreLinesLength == 0)
			{
				moreStartIndex = j + 1;

				/*如果左右已经完全匹配了,则需要再分段，这种情况只有完全是空白行的情况会出现
				*需要再分配。*/
				result.insert(i, BlockCmpPairInfo(1, i, j));
				break;
			}
			else if (lessLinesLength == 0)
			{
				//少边是0，则跳出，进行下一个行匹配。
				//20211108修改break为continue，发现匹配效果更好。修改为可配置。即空行参与匹配
				if (m_isBlankLineDoMatch)
				{
					continue;
				}
				else
				{
					break;
				}
			}
			else if (moreLinesLength == 0)
			{
				continue;
			}

			//做一个最大公共子串的算法
			int value = LcsTemplate<QChar>::getLcsLength(lessLinesLength, moreLinesLength, startLess, startMore);

			/*左右必须都要有一半以上的相等才叫相等，否则免谈;注意：这个值要做成动态配置。感觉默认40%即可*/
			float curEqualValue = value;
			curEqualValue /= (lessLinesLength + moreLinesLength);

			/*找最大匹配行，但是如果匹配率已经超过双方50%（该值可动态设置），则认定匹配，不再找最大值*/
			if(curEqualValue > maxValue)
			{
				maxValue = curEqualValue;
				//下次就要从后面继续开始了
				moreStartIndex = j + 1;

				//如果左右已经完全匹配了,则需要再分段，这种情况只有完全是空白行的情况会出现了
				if (value == moreLinesLength && value == lessLinesLength)
				{
					//需要再分配。
					result.insert(i, BlockCmpPairInfo(1, i, j));
					break;
				}
				//20220407 因为做了严格操作，所以不能进行最大匹配。必须要满足严格要求的比率后，才能看作匹配。
				//else
				//{
				//	//不是完全匹配的，但是最大匹配值,更新
				//	result.insert(i, BlockCmpPairInfo(2, i, j));
				//}

				if (m_lineMatchEqualRata == 50)
				{
					if (value >= lessLinesLength / 2 && value >= moreLinesLength / 2)
					{
						result.insert(i, BlockCmpPairInfo(2, i, j));
						break;
					}
				}
				else if(m_lineMatchEqualRata == 70)
				{
					if ((value >= lessLinesLength*0.7f)&& (value >= moreLinesLength*0.7f))
					{
						result.insert(i, BlockCmpPairInfo(2, i, j));
						break;
					}
				}
				else if (m_lineMatchEqualRata == 90)
				{
					if ((value >= lessLinesLength * 0.9f) && (value >= moreLinesLength * 0.9f))
					{
						result.insert(i, BlockCmpPairInfo(2, i, j));
						break;
					}
				}
				else
				{
					//不是完全匹配的，但是最大匹配值,更新
					result.insert(i, BlockCmpPairInfo(2, i, j));
					//注意这里没有break，是继续寻找最大值
				}

			}
		}
	}

	//如果结果为空，说明没有任何相等的子串，即每行都是完全不同的字符，这个几率很小
	//这种情况都可以理解为新增
	if (result.isEmpty())
	{
		createBlockWithMultiNoEqualLines(lessBlockInfo,lessLinesInfo, \
			moreBlockInfo, moreLinesInfo, \
			lessBlocks, moreBlocks);
	}
	else
	{
		int curLessNums = lessBlockInfo.startLine;
		int curMoreNums = moreBlockInfo.startLine;


		//结果不为空，说明有一些相等的子串
		for (QMap<int, BlockCmpPairInfo>::iterator it = result.begin(); it != result.end(); ++it)
		{
			if (m_isCancel != nullptr && *m_isCancel)
			{
				break;
			}

			//完全相等与部分匹配都是一样对待
			int lessLineNum = it->lessBlockLineNums;
			int moreLineNum = it->moreBlockLineNums;

			BlocksInfo lessBlockInfo_(false, curLessNums, lessLineNum, lessLineNum - curLessNums);
			BlocksInfo moreBlockInfo_(false, curMoreNums, moreLineNum, moreLineNum - curMoreNums);

			//先插入前面不能匹配的块
			createBlockWithMultiNoEqualLines(lessBlockInfo_, lessLinesInfo, \
				moreBlockInfo_, moreLinesInfo, \
				lessBlocks, moreBlocks);

			//再插入后面可以匹配的行
			createBlockWithOneLine(lessLineNum, moreLineNum, lessLinesInfo, moreLinesInfo, \
				lessBlocks, moreBlocks);

			curLessNums = lessLineNum + 1;
			curMoreNums = moreLineNum + 1;
		}

		//循环结束后，最后一块
		BlocksInfo lessBlockInfo_(false, curLessNums, lessBlockInfo.endLine, lessBlockInfo.endLine - curLessNums);
		BlocksInfo moreBlockInfo_(false, curMoreNums, moreBlockInfo.endLine, moreBlockInfo.endLine - curMoreNums);

		//先插入前面不相等的块
		createBlockWithMultiNoEqualLines(lessBlockInfo_, lessLinesInfo, \
			moreBlockInfo_, moreLinesInfo, \
			lessBlocks, moreBlocks);
	}
}
