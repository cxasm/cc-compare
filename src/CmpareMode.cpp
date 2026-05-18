#include "CmpareMode.h"
#include "Lcs.h"
#include "LcsLine.h"
#include "Encode.h"
#include "blockuserdata.h"
#include "rcglobal.h"
#include "comparewin.h"
#include "uchardet.h"

#include <QFile>
#include <QFileDevice> 
#include <QVector>
#include <QCryptographicHash>
#include <functional>
#include <QDataStream>
#include <QtConcurrent>
#include <string.h>


CmpareMode::CmpareMode(QString leftFilePath, QString rightFilePath):m_leftCurPos(0),m_rightCurPos(0), m_mode(0), m_leftFilePath(leftFilePath), m_rightFilePath(rightFilePath)
{
	m_leftFile = new QFile(leftFilePath);
	if (m_leftFile->open(QIODevice::ReadOnly))
	{
	m_leftContentLength = m_leftFile->size();

	m_leftFpr = m_leftFile->map(0, m_leftContentLength);

	//映射失败，发现空文件时，会出现映射失败的问题。
	if (m_leftFpr == nullptr)
	{
		//直接关闭文件，避免后续文件被占用。20230506发现的该问题。
		m_leftFile->close();
		delete m_leftFile;
		m_leftFile = nullptr;
	}
	}
	else
	{
		m_leftFpr = nullptr;
		m_leftFile = nullptr;
	}

	m_rightFile = new QFile(rightFilePath);
	if (m_rightFile->open(QIODevice::ReadOnly))
	{
	m_rightContentLength = m_rightFile->size();

	m_rightFpr = m_rightFile->map(0, m_rightContentLength);

	//映射失败，发现空文件时，会出现映射失败的问题。
	if (m_rightFpr == nullptr)
	{
		//直接关闭文件，避免后续文件被占用。20230506发现的该问题。
		m_rightFile->close();
		delete m_rightFile;
		m_rightFile = nullptr;
	}
	}
	else
	{
		m_rightFpr = nullptr;
		m_rightFile = nullptr;
	}

	m_lcsLine = nullptr;

	m_leftLinesNums = 0;
	m_rightLinesNums = 0;

	m_leftSkip = 0;
	m_rightSkip = 0;

	m_leftCode = CODE_ID::UNKOWN;
	m_rightCode = CODE_ID::UNKOWN;

	m_leftContent = new QStringList();
	m_rightContent = new QStringList();

	m_leftUnqealPosList.reserve(100);
	m_rightUnqealPosList.reserve(100);

}

//如果不是打开文件，而是直接拷贝内容进行对比。直接使用utf8编码
CmpareMode::CmpareMode(uchar* leftContent, int leftLens, uchar* rightContent, const int rightLens) :m_leftCurPos(0), m_rightCurPos(0), m_leftFile(nullptr), m_rightFile(nullptr),m_mode(0)
{
	m_leftFpr = leftContent;
	m_rightFpr = rightContent;

	m_leftContentLength = leftLens;
	m_rightContentLength = rightLens;

	m_lcsLine = nullptr;

	m_leftLinesNums = 0;
	m_rightLinesNums = 0;

	m_leftSkip = 0;
	m_rightSkip = 0;

	m_leftCode = CODE_ID::UTF8_NOBOM;
	m_rightCode = CODE_ID::UTF8_NOBOM;

	m_leftContent = new QStringList();
	m_rightContent = new QStringList();

	m_leftUnqealPosList.reserve(100);
	m_rightUnqealPosList.reserve(100);

}

CmpareMode::~CmpareMode()
{
	for (QList<BlockUserData*>::iterator it = m_leftExternBlockInfo.begin(); it != m_leftExternBlockInfo.end(); ++it)
	{
		delete* it;
	}
	m_leftExternBlockInfo.clear();

	for (QList<BlockUserData*>::iterator it = m_rightExternBlockInfo.begin(); it != m_rightExternBlockInfo.end(); ++it)
	{
		delete* it;
	}
	m_rightExternBlockInfo.clear();

	if (m_leftFpr != nullptr)
	{
		if (m_leftFile != nullptr)
		{
			m_leftFile->unmap(m_leftFpr);
			m_leftFile->close();
			delete m_leftFile;
			m_leftFile = nullptr;
		}
		m_leftFpr = nullptr;
	}

	if (m_rightFpr != nullptr)
	{
		if (m_rightFile != nullptr)
		{
			m_rightFile->unmap(m_leftFpr);
			m_rightFile->close();
			delete m_rightFile;
			m_rightFile = nullptr;
		}
		m_rightFpr = nullptr;
	}

	if (m_lcsLine != nullptr)
	{
		delete[]m_lcsLine;
		m_lcsLine = nullptr;
	}

	if (m_leftContent != nullptr)
	{
		delete m_leftContent;
		m_leftContent = nullptr;
	}

	if (m_rightContent != nullptr)
	{
		delete m_rightContent;
		m_rightContent = nullptr;
	}



}

//0 忽略行前行后空白 1 忽略行尾 2 忽略所有
void CmpareMode::setCmpMode(int mode)
{
	m_mode = mode;
}

void CmpareMode::setCmpTextCode(CODE_ID leftCode, CODE_ID rightCode)
{
	m_leftCode = leftCode;
	m_rightCode = rightCode;
}

//要释放文件，否则外面写文件时，不能清空文件，专门提供这个接口，在不同时关闭文件
void CmpareMode::releaseFile()
{
	if (m_leftFpr != nullptr)
	{
		if (m_leftFile != nullptr)
		{
			m_leftFile->unmap(m_leftFpr);
			m_leftFile->close();
			delete m_leftFile;
			m_leftFile = nullptr;
		}
		m_leftFpr = nullptr;
	}

	if (m_rightFpr != nullptr)
	{
		if (m_rightFile != nullptr)
		{
			m_rightFile->unmap(m_leftFpr);
			m_rightFile->close();
			delete m_rightFile;
			m_rightFile = nullptr;
		}
		m_rightFpr = nullptr;
	}
}

QFuture<ThreadFileCmpParameter*> CmpareMode::commitAsyncTask(std::function<ThreadFileCmpParameter*(ThreadFileCmpParameter*)> fun, ThreadFileCmpParameter* parameter)
{
	/* 这里最开始准备使用信号提交多线程，但是发现std:;function无法使用槽函数机制，需要自己是实现元对象
	* 直接使用QtConcurrent::run机制，不仅简单许多
	*/
	return QtConcurrent::run(fun, parameter);
}

//返回文本的内容是否相等。用在目录慢速对比文件时的情景
//完全时从work从仿照而来的。
bool CmpareMode::isFileTextEqual(AbstractCompare *blockCompare)
{
	//老的深入文本对比模式，发现太慢。
#if 0
	if (!work(blockCompare))
	{
		return false;
	}

	QList<NoEqualBlock> leftNoEqualBlocks;
	QList<NoEqualBlock> rightNoEqualBlocks;
	CompareWin::createNonEqualBlock(&m_leftExternBlockInfo, &m_rightExternBlockInfo, leftNoEqualBlocks, rightNoEqualBlocks);
	return (leftNoEqualBlocks.size() == 0)&&(rightNoEqualBlocks.size() == 0);
#else
	//20241108,直接使用cmpSkipEmptyLine就行。
#if 0
	//在对比的时候，不要针对每一个字符进行对比，而是针对每一行进行对比。这样效率会提高很多
	if (SCAN_SUCCESS != preScanFile(blockCompare))
	{
		//如果有一个是空文件，则直接返回空，不需要对比。目前只有可能是空文件才返回失败。
		//不是空文件，但是只有1个UTF-BOM，没有实际内容，也是空文件。直接在blockCompare进行了设置
		return false;
}

	//这里是按照行比较得到的LCS
	return isEqualByLine(blockCompare);
#endif

	return cmpSkipEmptyLine();
#endif
}

bool CmpareMode::work(AbstractCompare *blockCompare)
{
	//在对比的时候，不要针对每一个字符进行对比，而是针对每一行进行对比。这样效率会提高很多
	if (SCAN_SUCCESS != preScanFile(blockCompare))
	{
		//如果有一个是空文件，则直接返回空，不需要对比。目前只有可能是空文件才返回失败。
		//不是空文件，但是只有1个UTF-BOM，没有实际内容，也是空文件。直接在blockCompare进行了设置
		return false;
	}

	//这里是按照行比较得到的LCS
	cmpByLine(blockCompare);
	cmpAllBlockInfo();
	outputBlocks(blockCompare);

	emit outputMsg(1, tr("File Compare Finished !"));

	return true;
}

void CmpareMode::workBin(BinLcsResult *blockCompare)
{
	//如果有一个文件的长度是空，则返回空
	if (m_leftContentLength == 0 || m_rightContentLength == 0)
	{
		emit outputMsg(-1, "File is empty!");
		return;
	}
	else if ((m_leftContentLength > MAX_BIN_SIZE) || (m_rightContentLength > MAX_BIN_SIZE))
	{
		if (blockCompare->cmpMode == 0)
		{
			emit outputMsg(-1, tr("Error : Max Bin File Size is 10M ! Exceeding file size !"));
			return;
		}
		else if ((blockCompare->cmpMode == 1) && (blockCompare->leftStartPos == -1))
		{
			emit outputMsg(-1, tr("Error : Max Bin File Size is 10M ! Exceeding file size !"));
			return;
		}
	}

	if (blockCompare->cmpMode == 0)
	{

		cmp(blockCompare);

		if (blockCompare->isCmpCancel)
		{
			return;
		}

		emit moveStep();


		if (!blockCompare->isBlockCmp)
		{
			outputBinBlock(blockCompare);
		}
		else
		{
			outputBinMultiBlock(blockCompare);
		}
	}
	else if (blockCompare->cmpMode == 1)
	{
		//没有修改对比范围，对比整个文件
		if ((blockCompare->leftStartPos == -1))
		{
			outputOneByOneByte(blockCompare);
		}
		else
		{
			//对比文件中的一部分
			outputOneByOneByteWithPos(blockCompare);
		}
	}
}

//输出bin的块。把每个lcs当作一个间隔，依次就是不等/相等块的结果
void CmpareMode::outputBinBlock(BinLcsResult *blockCompare)
{
	blockCompare->leftStart = m_leftFpr;
	blockCompare->rightStart = m_rightFpr;

	blockCompare->leftSize = m_leftContentLength;
	blockCompare->rightSize = m_rightContentLength;


	uchar curChar;

	int curLeftPos = 0;
	int curRightPos = 0;

	//前面加入一个不等的块
	BinSectionNode unEqualSection;
	unEqualSection.equal = false;

	BinSectionNode equalSection;
	equalSection.equal = true;


	for (int i = 0; i < blockCompare->lcsLength; ++i)
	{
		curChar = *(blockCompare->lcsContents + i);

		for (; curLeftPos < blockCompare->leftSize; ++curLeftPos)
		{
			//找到第一个相等中的lcs的字符
			if (*(m_leftFpr + curLeftPos) == curChar)
			{
				equalSection.bytes.append(curChar);

				//找到退出时，把不等块和相等块加入。注意：就算没有不等块，这里也是加入一个空的不等块
				blockCompare->leftBinOut.append(unEqualSection);
				blockCompare->leftBinOut.append(equalSection);

				unEqualSection.bytes.clear();
				equalSection.bytes.clear();
				++curLeftPos;
				break;
			}
			else
			{
				//不等的字符，直接收集到不等块
				unEqualSection.bytes.append(*(m_leftFpr + curLeftPos));
			}
		}

		assert(unEqualSection.bytes.isEmpty());
		assert(equalSection.bytes.isEmpty());

		for (; curRightPos < blockCompare->rightSize; ++curRightPos)
		{
			//找到第一个相等中的lcs的字符
			if (*(m_rightFpr + curRightPos) == curChar)
			{
				equalSection.bytes.append(curChar);

				//找到退出时，把不等块和相等块加入。注意：就算没有不等块，这里也是加入一个空的不等块
				blockCompare->rightBinOut.append(unEqualSection);
				blockCompare->rightBinOut.append(equalSection);

				unEqualSection.bytes.clear();
				equalSection.bytes.clear();
				++curRightPos;

				break;
			}
			else
			{
				//不等的字符，直接收集到不等块
				unEqualSection.bytes.append(*(m_rightFpr + curRightPos));
			}
		}

		assert(unEqualSection.bytes.isEmpty());
		assert(equalSection.bytes.isEmpty());
	}

	emit moveStep();

	//处理lcs后面的字符

	unEqualSection.bytes.clear();

	for (; curLeftPos < blockCompare->leftSize; ++curLeftPos)
	{
		//不等的字符，直接收集到不等块
		unEqualSection.bytes.append(*(m_leftFpr + curLeftPos));
	}
	blockCompare->leftBinOut.append(unEqualSection);

	unEqualSection.bytes.clear();

	for (; curRightPos < blockCompare->rightSize; ++curRightPos)
	{
		//不等的字符，直接收集到不等块
		unEqualSection.bytes.append(*(m_rightFpr + curRightPos));
	}
	blockCompare->rightBinOut.append(unEqualSection);

	assert(blockCompare->leftBinOut.size() == blockCompare->rightBinOut.size());

	emit moveStep();


	//计算对齐后的长度
	int padLength = 0;
	QVector<int> everySectionLength;
	everySectionLength.reserve(blockCompare->leftBinOut.size());

	int leftTempLens = 0;
	int rightTempLens = 0;
	for (int i = 0; i < blockCompare->leftBinOut.size(); ++i)
	{
		leftTempLens = blockCompare->leftBinOut.at(i).bytes.length();
		rightTempLens = blockCompare->rightBinOut.at(i).bytes.length();

		if (leftTempLens >= rightTempLens)
		{
			padLength += leftTempLens;
			everySectionLength.append(leftTempLens);
		}
		else
		{
			padLength += rightTempLens;
			everySectionLength.append(rightTempLens);
		}
	}

	emit moveStep();

	const int ONE_LINE_CHAR_NUM = 16;

	//二进制.预留4倍空间，双字节+空格+字符显示就是4倍，还要前面的地址12+空格+换行2=15。预留16个
	//把左右的格式化进行输出。一个字符3个位置，还有一个字符，所有就是*4关系。最后保留1个16*4,是因为不足16字符的，尾巴对齐一下，所以保留48个字符。
	//后面2 * (padLength / ONE_LINE_CHAR_NUM + 1)还是预留了后续的换行。这里其实没有使用地址
	const int BUF_SIZE = padLength * 4 + 16 * 4 + 2 * (padLength / ONE_LINE_CHAR_NUM + 1);

	char* textOut = new char[BUF_SIZE];
	memset(textOut, 0, BUF_SIZE);
	int offset = 0;
	int index = 0;

	char* lineString = new char[ONE_LINE_CHAR_NUM+1];
	memset(lineString, 0, ONE_LINE_CHAR_NUM+1);

	int lineMax = 0;

	BinUnequalPos unequalPos;

	//获取需要跳过的不相等区域
	const int ONE_LINE_DISPLAY_LENS = ONE_LINE_CHAR_NUM * 4 + 2; //加2是因为尾巴换行符号
	auto getSkipUnequalRange = [=](BinUnequalPos& unequalRange, QVector<BinUnequalPos>& output) {
		//1行有ONE_LINE_CHAR_NUM个字符，每个字符3个显示为，字符显示区域为ONE_LINE_CHAR_NUM*3，文本显示区域为ONE_LINE_CHAR_NUM*3->ONE_LINE_CHAR_NUM*4
		int beginPos = unequalRange.start / ONE_LINE_DISPLAY_LENS;
		int endPos = unequalRange.end / ONE_LINE_DISPLAY_LENS;
		int lastEndPos = unequalRange.end % ONE_LINE_DISPLAY_LENS;

		//没有占到文本显示区域，不需要跳过
		if ((beginPos == endPos) && (lastEndPos < ONE_LINE_CHAR_NUM * 3))
		{
			return;
		}

		BinUnequalPos skipRange;

		for (int i = beginPos; i <= endPos; ++i)
		{
			skipRange.start = i * ONE_LINE_DISPLAY_LENS + ONE_LINE_CHAR_NUM*3;

			if (skipRange.start >= unequalRange.end)
			{
				break;
			}

			skipRange.end = skipRange.start + ONE_LINE_CHAR_NUM + 2;

			if (skipRange.end > unequalRange.end)
			{
				skipRange.end = unequalRange.end;
			}
			output.append(skipRange);
		}
	};


	auto outputTextContents = [&](int lens) {
		memcpy(textOut + offset, lineString, lens);
		offset += lens;
		memset(lineString, 0, ONE_LINE_CHAR_NUM + 1);

		if (lens < ONE_LINE_CHAR_NUM)
		{
			memset(textOut + offset,' ', ONE_LINE_CHAR_NUM-lens);

			offset += ONE_LINE_CHAR_NUM - lens;
		}
		lineMax = 0;

		sprintf(textOut + offset, "\r\n");
		offset += 2;
		index = 0;
	};

	//顺序是不等/相等依次排列，第一个块一定是不等。最后一个一定是个不等。哪怕是空块
	for (int i = 0, size = blockCompare->leftBinOut.size(); i < size; ++i)
	{
		//偶数的序号块，就是不等块，把不等块的起始位置获取，并搜集到容器中。
		//后面需要标识这个块中字符的颜色为红色
		if (i % 2 == 0)
		{
			unequalPos.start = offset;
		}

		const BinSectionNode& section = blockCompare->leftBinOut.at(i);

		for (int j = 0; j < section.bytes.length(); ++j)
		{
			uchar c = section.bytes.at(j);

			sprintf(textOut + offset, "%02X ", c);
			offset += 3;
			index++;

			//如果在可显示字符内
			if (c >= 32 && c <= 126)
			{
				lineString[lineMax] = c;
			}
			else
			{
				lineString[lineMax] = ' ';
			}

			lineMax++;

			if (index == ONE_LINE_CHAR_NUM)
			{
				/*memcpy(textOut + offset, lineString, ONE_LINE_CHAR_NUM);
				offset += ONE_LINE_CHAR_NUM;
				memset(lineString, 0, ONE_LINE_CHAR_NUM+1);
				lineMax = 0;

				sprintf(textOut + offset, "\r\n");
				offset += 2;
				index = 0;*/
				outputTextContents(index);
			}
		}

		//按照左右中更长的去对齐长度
		if (section.bytes.length() < everySectionLength.at(i))
		{
			for (int j = section.bytes.length(), s = everySectionLength.at(i); j < s; ++j)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
				index++;

				lineString[lineMax] = ' ';
				lineMax++;

				if (index == ONE_LINE_CHAR_NUM)
				{
					/*memcpy(textOut + offset, lineString, ONE_LINE_CHAR_NUM);
					offset += ONE_LINE_CHAR_NUM;
					memset(lineString, 0, ONE_LINE_CHAR_NUM+1);
					lineMax = 0;

					sprintf(textOut + offset, "\r\n");
					offset += 2;
					index = 0;*/
					outputTextContents(index);
				}
			}
		}

		//最后1块做对齐处理
		if ((i == (size-1)) && (index > 0))
		{
			//对于尾巴不慢16字符的，对齐一下，让文本总是显示在最后的16个空间上
			for (int m = 0; m < (ONE_LINE_CHAR_NUM - index); ++m)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
			}
			outputTextContents(index);
		}

		if (i % 2 == 0)
		{
			unequalPos.end = offset;

			blockCompare->leftUnequalPos.append(unequalPos);

			//因为不等块中包含了尾巴16个字符的文字块，这部分要从不等块中剔除
			getSkipUnequalRange(unequalPos, blockCompare->leftSkipUnequalPos);
		}
	}



	blockCompare->leftPrintContents += QString(textOut);

	emit moveStep();

	//把左右的格式化进行输出

	memset(textOut, 0, BUF_SIZE);
	offset = 0;
	index = 0;

	lineMax = 0;

	for (int i = 0, size = blockCompare->rightBinOut.size(); i < size; ++i)
	{
		if (i % 2 == 0)
		{
			unequalPos.start = offset;
		}

		const BinSectionNode& section = blockCompare->rightBinOut.at(i);

		for (int j = 0; j < section.bytes.length(); ++j)
		{
			uchar c = section.bytes.at(j);

			sprintf(textOut + offset, "%02X ", c);
			offset += 3;
			index++;

			//如果在可显示字符内
			if (c >= 32 && c <= 126)
			{
				lineString[lineMax] = c;
			}
			else
			{
				lineString[lineMax] = ' ';
			}

			lineMax++;

			if (index == ONE_LINE_CHAR_NUM)
			{
				/*memcpy(textOut + offset, lineString, ONE_LINE_CHAR_NUM);
				offset += ONE_LINE_CHAR_NUM;
				memset(lineString, 0, ONE_LINE_CHAR_NUM+1);
				lineMax = 0;

				sprintf(textOut + offset, "\r\n");
				offset += 2;
				index = 0;*/
				outputTextContents(index);
			}
		}

		//按照左右中更长的去对齐长度
		if (section.bytes.length() < everySectionLength.at(i))
		{
			for (int j = section.bytes.length(), s = everySectionLength.at(i); j < s; ++j)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
				index++;

				lineString[lineMax] = ' ';
				lineMax++;

				if (index == ONE_LINE_CHAR_NUM)
				{
					/*memcpy(textOut + offset, lineString, ONE_LINE_CHAR_NUM);
					offset += ONE_LINE_CHAR_NUM;
					memset(lineString, 0, ONE_LINE_CHAR_NUM+1);
					lineMax = 0;

					sprintf(textOut + offset, "\r\n");
					offset += 2;
					index = 0;*/
					outputTextContents(index);
				}
			}
		}

		//最后1块做对齐处理
		if ((i == (size - 1)) && (index > 0))
		{
			//对于尾巴不慢16字符的，对齐一下，让文本总是显示在最后的16个空间上
			for (int m = 0; m < (ONE_LINE_CHAR_NUM - index); ++m)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
			}
			outputTextContents(index);
		}

		if (i % 2 == 0)
		{
			unequalPos.end = offset;

			blockCompare->rightUnequalPos.append(unequalPos);
			getSkipUnequalRange(unequalPos, blockCompare->rightSkipUnequalPos);
		}
	}

	blockCompare->rightPrintContents += QString(textOut);

	emit moveStep();

	delete[]textOut;
	delete[]lineString;

}

//给1对1byte来进行对比的函数
int CmpareMode::outputBinBlock(int leftSize, int rightSize, int leftOffet, int rightOffet, uchar* leftStr, uchar* rightStr, \
	QVector<BinUnequalPos> &leftUnequalPos, QVector<BinUnequalPos> &rightUnequalPos, QVector<BinUnequalPos>& leftSkipUnequalPos, QVector<BinUnequalPos>& rightSkipUnequalPos, \
	QString& leftPrintContents, QString& rightPrintContents, int &curLeftOffset, int &curRightOffset)
{

	uchar curChar;

	int curLeftPos = 0;
	int curRightPos = 0;

	//前面加入一个不等的块
	BinSectionNode unEqualSectionLeft;
	unEqualSectionLeft.equal = false;

	BinSectionNode unEqualSectionRight;
	unEqualSectionRight.equal = false;

	BinSectionNode equalSectionLeft;
	equalSectionLeft.equal = true;

	BinSectionNode equalSectionRight;
	equalSectionRight.equal = true;

	QVector<BinSectionNode>leftBinOut;
	QVector<BinSectionNode>rightBinOut;

	int equalBytesNum = 0;


	//根据左右字符是否相等，把内容分割成不等-相等的一些列段落
	int minLength = ((leftSize >= rightSize) ? rightSize : leftSize);

	//是否相等
	bool isPreEqual = false;

	//第一个一定是一个不等块，如果第一个字符就是相等，也会无条件加入一个空的不等块
	for (int i = 0; i < minLength; ++i)
	{
		curChar = *(leftStr + i);

		if (curChar == *(rightStr + i))
		{
			++equalBytesNum;

			equalSectionLeft.bytes.append(curChar);
			equalSectionRight.bytes.append(curChar);

			if (!isPreEqual)
			{
				//前面是不等的，则前面不等的结束，加入不等快
				leftBinOut.append(unEqualSectionLeft);
				rightBinOut.append(unEqualSectionRight);

				unEqualSectionLeft.bytes.clear();
				unEqualSectionRight.bytes.clear();
				isPreEqual = true;
			}
		}
		else
		{

			unEqualSectionLeft.bytes.append(*(leftStr + i));
			unEqualSectionRight.bytes.append(*(rightStr + i));

			if (isPreEqual)
			{
				//前面是相等的，则前面相等的结束，加入相等块
				leftBinOut.append(equalSectionLeft);
				rightBinOut.append(equalSectionRight);

				equalSectionLeft.bytes.clear();
				equalSectionRight.bytes.clear();
				isPreEqual = false;
			}
		}
	}

	auto appendTail = [&](bool isClear) {

		if (isClear)
		{
			unEqualSectionLeft.bytes.clear();
			unEqualSectionRight.bytes.clear();
		}

		if (minLength < leftSize)
		{
			for (curLeftPos = minLength; curLeftPos < leftSize; ++curLeftPos)
			{
				//不等的字符，直接收集到不等块
				unEqualSectionLeft.bytes.append(*(leftStr + curLeftPos));
			}
		}
		else if (minLength < rightSize)
		{
			for (curRightPos = minLength; curRightPos < rightSize; ++curRightPos)
			{
				//不等的字符，直接收集到不等块
				unEqualSectionRight.bytes.append(*(rightStr + curRightPos));
			}
		}

		//注意这里是无条件收集最后的块的，哪怕是空块。一定是不等-相等这样的，最后一个一定是不等，哪怕是空块
		leftBinOut.append(unEqualSectionLeft);
		rightBinOut.append(unEqualSectionRight);
	};

	//结尾后，因为状态没有变化，还有残留的没有加入到收集中
	//如果退出时，前面是相等块，则可能前面的相等块没有加入
	if (isPreEqual)
	{
		if (!equalSectionLeft.bytes.isEmpty())
		{
			leftBinOut.append(equalSectionLeft);
			rightBinOut.append(equalSectionRight);

			//最后1个是相等块，则尾部加入不等块，哪怕是空块
			appendTail(true);
		}
		else
		{
			//最后1个虽然是相等，但是相等的是空的，即其实最后1个是不等，则追加
			//理论上这里永远不该发生
			assert(false);
		}
	}
	else
	{
		if (!unEqualSectionLeft.bytes.isEmpty())
		{
			//最后一个是不等，则直接把尾部可能不等的部分追加到最后1个不等块后面
			appendTail(false);
		}
		else
		{
			//同上
			assert(false);
		}
	}

	emit moveStep();


	//计算对齐后的长度
	int padLength = ((leftSize >= rightSize) ? leftSize : rightSize);

	int leftPadLength = padLength - leftSize;
	int rightPadLength = padLength - rightSize;

	assert(leftBinOut.size() == rightBinOut.size());

	emit moveStep();

	const int ONE_LINE_CHAR_NUM = 16;

	//二进制.预留4倍空间，双字节+空格+字符显示就是4倍，还要前面的地址8+空格+换行2=11。预留12个
	//把左右的格式化进行输出。一个字符3个位置，还有一个字符，所有就是*4关系。最后保留1个16*4,是因为不足16字符的，尾巴对齐一下，所以保留48个字符。
	//后面12 * (padLength / ONE_LINE_CHAR_NUM + 1)前面的地址8+空格+换行2=11。预留12个

	const int BUF_SIZE = padLength * 4 + 16 * 4 + 12 * (padLength / ONE_LINE_CHAR_NUM + 1);

	char* textOut = new char[BUF_SIZE];
	memset(textOut, 0, BUF_SIZE);
	int offset = 0;
	int index = 0;

	char* lineString = new char[ONE_LINE_CHAR_NUM + 1];
	memset(lineString, 0, ONE_LINE_CHAR_NUM + 1);

	int lineMax = 0;

	BinUnequalPos unequalPos;

	//获取需要跳过的不相等区域
	const int ONE_LINE_DISPLAY_LENS = ONE_LINE_CHAR_NUM * 4 + 2; // 2是因为尾巴换行符号
	auto getSkipUnequalRange = [=](BinUnequalPos& unequalRange, QVector<BinUnequalPos>& output) {
		//1行有ONE_LINE_CHAR_NUM个字符，每个字符3个显示为，字符显示区域为ONE_LINE_CHAR_NUM*3，文本显示区域为ONE_LINE_CHAR_NUM*3->ONE_LINE_CHAR_NUM*4
		int beginPos = unequalRange.start / ONE_LINE_DISPLAY_LENS;
		int endPos = unequalRange.end / ONE_LINE_DISPLAY_LENS;
		int lastEndPos = unequalRange.end % ONE_LINE_DISPLAY_LENS;

		//没有占到文本显示区域，不需要跳过
		if ((beginPos == endPos) && (lastEndPos < ONE_LINE_CHAR_NUM * 3))
		{
			return;
		}

		BinUnequalPos skipRange;

		for (int i = beginPos; i <= endPos; ++i)
		{
			skipRange.start = i * ONE_LINE_DISPLAY_LENS + ONE_LINE_CHAR_NUM * 3;

			if (skipRange.start >= unequalRange.end)
			{
				break;
			}

			//加2时因为16个文字后面，还有换行符号
			skipRange.end = skipRange.start + ONE_LINE_CHAR_NUM +2;

			if (skipRange.end > unequalRange.end)
			{
				skipRange.end = unequalRange.end;
			}
			output.append(skipRange);
		}
	};


	auto outputTextContents = [&](int lens) {
		memcpy(textOut + offset, lineString, lens);
		offset += lens;
		memset(lineString, 0, ONE_LINE_CHAR_NUM + 1);

		if (lens < ONE_LINE_CHAR_NUM)
		{
			memset(textOut + offset, ' ', ONE_LINE_CHAR_NUM - lens);

			offset += ONE_LINE_CHAR_NUM - lens;
		}
		lineMax = 0;

		sprintf(textOut + offset, "\r\n");
		offset += 2;
		index = 0;
	};

	//顺序是不等/相等依次排列，第一个块一定是不等。最后一个一定是个不等。哪怕是空块
	for (int i = 0, size = leftBinOut.size(); i < size; ++i)
	{

		//偶数的序号块，就是不等块，把不等块的起始位置获取，并搜集到容器中。
		//后面需要标识这个块中字符的颜色为红色
		if (i % 2 == 0)
		{
			unequalPos.start = offset + leftOffet;
		}

		const BinSectionNode& section = leftBinOut.at(i);

		for (int j = 0; j < section.bytes.length(); ++j)
		{
			uchar c = section.bytes.at(j);

			sprintf(textOut + offset, "%02X ", c);
			offset += 3;
			index++;

			//如果在可显示字符内
			if (c >= 32 && c <= 126)
			{
				lineString[lineMax] = c;
			}
			else
			{
				lineString[lineMax] = ' ';
			}

			lineMax++;

			if (index == ONE_LINE_CHAR_NUM)
			{
				outputTextContents(index);
			}
		}

		//最后一块需要做补齐处理
		if ((i == (size - 1)) && (leftPadLength > 0))
		{
			for (int j = 0; j < leftPadLength; ++j)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
				index++;

				lineString[lineMax] = ' ';
				lineMax++;

				if (index == ONE_LINE_CHAR_NUM)
				{
					outputTextContents(index);
				}
			}
		}

		//最后1块做对齐处理
		if ((i == (size - 1)) && (index > 0))
		{
			//对于尾巴不慢16字符的，对齐一下，让文本总是显示在最后的16个空间上
			for (int m = 0; m < (ONE_LINE_CHAR_NUM - index); ++m)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
			}
			outputTextContents(index);
		}

		if (i % 2 == 0)
		{
			unequalPos.end = offset + leftOffet;

			leftUnequalPos.append(unequalPos);

			//因为不等块中包含了尾巴16个字符的文字块，这部分要从不等块中剔除
			getSkipUnequalRange(unequalPos, leftSkipUnequalPos);
		}
	}

	curLeftOffset = offset + leftOffet;
	leftPrintContents += QString(textOut);

	emit moveStep();

	//把左右的格式化进行输出

	memset(textOut, 0, BUF_SIZE);
	offset = 0;
	index = 0;

	lineMax = 0;

	for (int i = 0, size = rightBinOut.size(); i < size; ++i)
	{
		if (i % 2 == 0)
		{
			unequalPos.start = offset + rightOffet;
		}

		const BinSectionNode& section = rightBinOut.at(i);

		for (int j = 0; j < section.bytes.length(); ++j)
		{
			uchar c = section.bytes.at(j);

			sprintf(textOut + offset, "%02X ", c);
			offset += 3;
			index++;

			//如果在可显示字符内
			if (c >= 32 && c <= 126)
			{
				lineString[lineMax] = c;
			}
			else
			{
				lineString[lineMax] = ' ';
			}

			lineMax++;

			if (index == ONE_LINE_CHAR_NUM)
			{
				outputTextContents(index);
			}
		}

		//最后一块需要做补齐处理
		if ((i == (size - 1)) && (rightPadLength > 0))
		{
			for (int j = 0; j < rightPadLength; ++j)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
				index++;

				lineString[lineMax] = ' ';
				lineMax++;

				if (index == ONE_LINE_CHAR_NUM)
				{
					outputTextContents(index);
				}
			}
		}

		//最后1块做对齐处理
		if ((i == (size - 1)) && (index > 0))
		{
			//对于尾巴不慢16字符的，对齐一下，让文本总是显示在最后的16个空间上
			for (int m = 0; m < (ONE_LINE_CHAR_NUM - index); ++m)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
			}
			outputTextContents(index);
		}

		if (i % 2 == 0)
		{
			unequalPos.end = offset + rightOffet;
			rightUnequalPos.append(unequalPos);
			getSkipUnequalRange(unequalPos, rightSkipUnequalPos);
		}
	}

	curRightOffset = offset + rightOffet;

	rightPrintContents += QString(textOut);

	emit moveStep();

	delete[]textOut;
	delete[]lineString;

	return equalBytesNum;
}

//用于一对一字节对比的函数。
//leftSize : 左边的大小（当前块大小，而不是全部文件）
//leftOffet: 当前文件的偏移量。忘记了，可能是当前在输入框中的偏移量
//leftStr：内存块开始值，外面已经做过块偏移
//leftUnequalPos::不等块的区间容器
//leftSkipUnequalPos:需要跳过的不等快部分，就是右边的文字，同一设置为黑色
//leftPrintContents:输出内容字符串
//curLeftOffset：当前偏移量，是个输出，是为了给输入参数leftOffet在外面做输出使用的

void CmpareMode::outputBinBlock(int lcsLength, int leftSize, int rightSize, int leftOffet, int rightOffet, uchar* lcsContents, uchar* leftStr, uchar* rightStr,\
	QVector<BinUnequalPos> &leftUnequalPos,QVector<BinUnequalPos> &rightUnequalPos,QVector<BinUnequalPos>& leftSkipUnequalPos,QVector<BinUnequalPos>& rightSkipUnequalPos,\
	QString& leftPrintContents, QString& rightPrintContents,int &curLeftOffset, int &curRightOffset)
{

	uchar curChar;

	int curLeftPos = 0;
	int curRightPos = 0;

	//前面加入一个不等的块
	BinSectionNode unEqualSection;
	unEqualSection.equal = false;

	BinSectionNode equalSection;
	equalSection.equal = true;

	QVector<BinSectionNode>leftBinOut;
	QVector<BinSectionNode>rightBinOut;

	//根据lcs把内容分割成不等-相等的一些列段落
	for (int i = 0; i < lcsLength; ++i)
	{
		curChar = *(lcsContents + i);

		for (; curLeftPos < leftSize; ++curLeftPos)
		{
			//找到第一个相等中的lcs的字符
			if (*(leftStr + curLeftPos) == curChar)
			{
				equalSection.bytes.append(curChar);

				//找到退出时，把不等块和相等块加入。注意：就算没有不等块，这里也是加入一个空的不等块
				leftBinOut.append(unEqualSection);
				leftBinOut.append(equalSection);

				unEqualSection.bytes.clear();
				equalSection.bytes.clear();
				++curLeftPos;
				break;
			}
			else
			{
				//不等的字符，直接收集到不等块
				unEqualSection.bytes.append(*(leftStr + curLeftPos));
			}
		}

		assert(unEqualSection.bytes.isEmpty());
		assert(equalSection.bytes.isEmpty());

		for (; curRightPos < rightSize; ++curRightPos)
		{
			//找到第一个相等中的lcs的字符
			if (*(rightStr + curRightPos) == curChar)
			{
				equalSection.bytes.append(curChar);

				//找到退出时，把不等块和相等块加入。注意：就算没有不等块，这里也是加入一个空的不等块
				rightBinOut.append(unEqualSection);
				rightBinOut.append(equalSection);

				unEqualSection.bytes.clear();
				equalSection.bytes.clear();
				++curRightPos;
				break;
			}
			else
			{
				//不等的字符，直接收集到不等块
				unEqualSection.bytes.append(*(rightStr + curRightPos));
			}
		}
		assert(unEqualSection.bytes.isEmpty());
		assert(equalSection.bytes.isEmpty());
	}

	emit moveStep();

	//处理lcs后面的字符

	unEqualSection.bytes.clear();

	for (; curLeftPos < leftSize; ++curLeftPos)
	{
		//不等的字符，直接收集到不等块
		unEqualSection.bytes.append(*(leftStr + curLeftPos));
	}
	leftBinOut.append(unEqualSection);

	unEqualSection.bytes.clear();

	for (; curRightPos < rightSize; ++curRightPos)
	{
		//不等的字符，直接收集到不等块
		unEqualSection.bytes.append(*(rightStr + curRightPos));
	}
	rightBinOut.append(unEqualSection);

	assert(leftBinOut.size() == rightBinOut.size());

	emit moveStep();


	//计算对齐后的长度
	int padLength = 0;
	QVector<int> everySectionLength;
	everySectionLength.reserve(leftBinOut.size());

	int leftTempLens = 0;
	int rightTempLens = 0;
	for (int i = 0; i < leftBinOut.size(); ++i)
	{
		leftTempLens = leftBinOut.at(i).bytes.length();
		rightTempLens = rightBinOut.at(i).bytes.length();

		if (leftTempLens >= rightTempLens)
		{
			padLength += leftTempLens;
			everySectionLength.append(leftTempLens);
		}
		else
		{
			padLength += rightTempLens;
			everySectionLength.append(rightTempLens);
		}
	}

	emit moveStep();

	const int ONE_LINE_CHAR_NUM = 16;

	//二进制.预留4倍空间，双字节+空格+字符显示就是4倍，还要前面的地址12+空格+换行2=15。预留16个
	//把左右的格式化进行输出。一个字符3个位置，还有一个字符，所有就是*4关系。最后保留1个16*4,是因为不足16字符的，尾巴对齐一下，所以保留48个字符。
	//后面2 * (padLength / ONE_LINE_CHAR_NUM + 1)还是预留了后续的换行。这里其实没有使用地址
	const int BUF_SIZE = padLength * 4 + 16 * 4 + 2 * (padLength / ONE_LINE_CHAR_NUM + 1);

	char* textOut = new char[BUF_SIZE];
	memset(textOut, 0, BUF_SIZE);
	int offset = 0;
	int index = 0;

	char* lineString = new char[ONE_LINE_CHAR_NUM + 1];
	memset(lineString, 0, ONE_LINE_CHAR_NUM + 1);

	int lineMax = 0;

	BinUnequalPos unequalPos;

	//获取需要跳过的不相等区域
	const int ONE_LINE_DISPLAY_LENS = ONE_LINE_CHAR_NUM * 4 + 2; //加2是因为尾巴换行符号
	auto getSkipUnequalRange = [=](BinUnequalPos& unequalRange, QVector<BinUnequalPos>& output) {
		//1行有ONE_LINE_CHAR_NUM个字符，每个字符3个显示为，字符显示区域为ONE_LINE_CHAR_NUM*3，文本显示区域为ONE_LINE_CHAR_NUM*3->ONE_LINE_CHAR_NUM*4
		int beginPos = unequalRange.start / ONE_LINE_DISPLAY_LENS;
		int endPos = unequalRange.end / ONE_LINE_DISPLAY_LENS;
		int lastEndPos = unequalRange.end % ONE_LINE_DISPLAY_LENS;

		//没有占到文本显示区域，不需要跳过
		if ((beginPos == endPos) && (lastEndPos < ONE_LINE_CHAR_NUM * 3))
		{
			return;
		}

		BinUnequalPos skipRange;

		for (int i = beginPos; i <= endPos; ++i)
		{
			skipRange.start = i * ONE_LINE_DISPLAY_LENS + ONE_LINE_CHAR_NUM * 3;

			if (skipRange.start >= unequalRange.end)
			{
				break;
			}

			skipRange.end = skipRange.start + ONE_LINE_CHAR_NUM + 2;

			if (skipRange.end > unequalRange.end)
			{
				skipRange.end = unequalRange.end;
			}
			output.append(skipRange);
		}
	};


	auto outputTextContents = [&](int lens) {
		memcpy(textOut + offset, lineString, lens);
		offset += lens;
		memset(lineString, 0, ONE_LINE_CHAR_NUM + 1);

		if (lens < ONE_LINE_CHAR_NUM)
		{
			memset(textOut + offset, ' ', ONE_LINE_CHAR_NUM - lens);

			offset += ONE_LINE_CHAR_NUM - lens;
		}
		lineMax = 0;

		sprintf(textOut + offset, "\r\n");
		offset += 2;
		index = 0;
	};

	//顺序是不等/相等依次排列，第一个块一定是不等。最后一个一定是个不等。哪怕是空块
	for (int i = 0, size = leftBinOut.size(); i < size; ++i)
	{
		//偶数的序号块，就是不等块，把不等块的起始位置获取，并搜集到容器中。
		//后面需要标识这个块中字符的颜色为红色
		if (i % 2 == 0)
		{
			unequalPos.start = offset + leftOffet;
		}

		const BinSectionNode& section = leftBinOut.at(i);

		for (int j = 0; j < section.bytes.length(); ++j)
		{
			uchar c = section.bytes.at(j);

			sprintf(textOut + offset, "%02X ", c);
			offset += 3;
			index++;

			//如果在可显示字符内
			if (c >= 32 && c <= 126)
			{
				lineString[lineMax] = c;
			}
			else
			{
				lineString[lineMax] = ' ';
			}

			lineMax++;

			if (index == ONE_LINE_CHAR_NUM)
			{
				outputTextContents(index);
			}
		}

		//按照左右中更长的去对齐长度
		if (section.bytes.length() < everySectionLength.at(i))
		{
			for (int j = section.bytes.length(), s = everySectionLength.at(i); j < s; ++j)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
				index++;

				lineString[lineMax] = ' ';
				lineMax++;

				if (index == ONE_LINE_CHAR_NUM)
				{
					outputTextContents(index);
				}
			}
		}

		//最后1块做对齐处理
		if ((i == (size - 1)) && (index > 0))
		{
			//对于尾巴不慢16字符的，对齐一下，让文本总是显示在最后的16个空间上
			for (int m = 0; m < (ONE_LINE_CHAR_NUM - index); ++m)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
			}
			outputTextContents(index);
		}

		if (i % 2 == 0)
		{
			unequalPos.end = offset + leftOffet;

			leftUnequalPos.append(unequalPos);

			//因为不等块中包含了尾巴16个字符的文字块，这部分要从不等块中剔除
			getSkipUnequalRange(unequalPos, leftSkipUnequalPos);
		}
	}

	curLeftOffset = offset + leftOffet;
	leftPrintContents += QString(textOut);

	emit moveStep();

	//把左右的格式化进行输出

	memset(textOut, 0, BUF_SIZE);
	offset = 0;
	index = 0;

	lineMax = 0;

	for (int i = 0, size = rightBinOut.size(); i < size; ++i)
	{
		if (i % 2 == 0)
		{
			unequalPos.start = offset + rightOffet;
		}

		const BinSectionNode& section = rightBinOut.at(i);

		for (int j = 0; j < section.bytes.length(); ++j)
		{
			uchar c = section.bytes.at(j);

			sprintf(textOut + offset, "%02X ", c);
			offset += 3;
			index++;

			//如果在可显示字符内
			if (c >= 32 && c <= 126)
			{
				lineString[lineMax] = c;
			}
			else
			{
				lineString[lineMax] = ' ';
			}

			lineMax++;

			if (index == ONE_LINE_CHAR_NUM)
			{
				outputTextContents(index);
			}
		}

		//按照左右中更长的去对齐长度
		if (section.bytes.length() < everySectionLength.at(i))
		{
			for (int j = section.bytes.length(), s = everySectionLength.at(i); j < s; ++j)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
				index++;

				lineString[lineMax] = ' ';
				lineMax++;

				if (index == ONE_LINE_CHAR_NUM)
				{
					outputTextContents(index);
				}
			}
		}

		//最后1块做对齐处理
		if ((i == (size - 1)) && (index > 0))
		{
			//对于尾巴不慢16字符的，对齐一下，让文本总是显示在最后的16个空间上
			for (int m = 0; m < (ONE_LINE_CHAR_NUM - index); ++m)
			{
				sprintf(textOut + offset, "-- ");
				offset += 3;
			}
			outputTextContents(index);
		}

		if (i % 2 == 0)
		{
			unequalPos.end = offset + rightOffet;
			rightUnequalPos.append(unequalPos);
			getSkipUnequalRange(unequalPos, rightSkipUnequalPos);
		}
	}

	curRightOffset = offset + rightOffet;

	rightPrintContents += QString(textOut);

	emit moveStep();

	delete[]textOut;
	delete[]lineString;

}

//对比整个文件。
void CmpareMode::outputOneByOneByte(BinLcsResult *blockCompare)
{
	blockCompare->leftStart = m_leftFpr;
	blockCompare->rightStart = m_rightFpr;

	blockCompare->leftSize = m_leftContentLength;
	blockCompare->rightSize = m_rightContentLength;

	int curLeftOffset = 0;
	int curRightOffset = 0;

	QVector<BinUnequalPos>& leftUnequalPos = blockCompare->leftUnequalPos;
	QVector<BinUnequalPos>& rightUnequalPos = blockCompare->rightUnequalPos;
	QVector<BinUnequalPos>& leftSkipUnequalPos = blockCompare->leftSkipUnequalPos;
	QVector<BinUnequalPos>& rightSkipUnequalPos = blockCompare->rightSkipUnequalPos;;
	QString& leftPrintContents = blockCompare->leftPrintContents;
	QString& rightPrintContents = blockCompare->rightPrintContents;

	//这里其实不是lcs的长度，就是一对一对比时相等的字节数量。
	blockCompare->lcsLength = outputBinBlock(blockCompare->leftSize, blockCompare->rightSize, curLeftOffset, curRightOffset, m_leftFpr, m_rightFpr, \
			leftUnequalPos, rightUnequalPos, leftSkipUnequalPos, rightSkipUnequalPos, \
			leftPrintContents, rightPrintContents, curLeftOffset, curRightOffset);

}

//对比对比其中一块，而不是对比整个文件。
void CmpareMode::outputOneByOneByteWithPos(BinLcsResult *blockCompare)
{

	blockCompare->leftSize = m_leftContentLength;
	blockCompare->rightSize = m_rightContentLength;

	//检查参数范围
	if ((blockCompare->leftStartPos >= blockCompare->leftSize) || (blockCompare->rightStartPos >= blockCompare->rightSize))
	{
		emit outputMsg(-1,tr("File Start Pos Exceeding File Size !"));
		return;
	}

	blockCompare->leftStart = m_leftFpr + blockCompare->leftStartPos;
	blockCompare->rightStart = m_rightFpr + blockCompare->rightStartPos;

	//对比范围不能超过文件大小，避免文件越界访问
	if (blockCompare->leftStartPos + blockCompare->leftCmpLen > blockCompare->leftSize)
	{
		blockCompare->leftCmpLen = blockCompare->leftSize - blockCompare->leftStartPos;
	}
	if (blockCompare->rightStartPos + blockCompare->rightCmpLen > blockCompare->rightSize)
	{
		blockCompare->rightCmpLen = blockCompare->rightSize - blockCompare->rightStartPos;
	}


	int curLeftOffset = 0;
	int curRightOffset = 0;

	QVector<BinUnequalPos>& leftUnequalPos = blockCompare->leftUnequalPos;
	QVector<BinUnequalPos>& rightUnequalPos = blockCompare->rightUnequalPos;
	QVector<BinUnequalPos>& leftSkipUnequalPos = blockCompare->leftSkipUnequalPos;
	QVector<BinUnequalPos>& rightSkipUnequalPos = blockCompare->rightSkipUnequalPos;;
	QString& leftPrintContents = blockCompare->leftPrintContents;
	QString& rightPrintContents = blockCompare->rightPrintContents;

	//这里其实不是lcs的长度，就是一对一对比时相等的字节数量。
	blockCompare->lcsLength = outputBinBlock(blockCompare->leftCmpLen, blockCompare->rightCmpLen, curLeftOffset, curRightOffset, blockCompare->leftStart, blockCompare->rightStart, \
		leftUnequalPos, rightUnequalPos, leftSkipUnequalPos, rightSkipUnequalPos, \
		leftPrintContents, rightPrintContents, curLeftOffset, curRightOffset);

}

//输出bin的块。把每个lcs当作一个间隔，依次就是不等/相等块的结果
//每次按照8k进行的lcs分块输出
void CmpareMode::outputBinMultiBlock(BinLcsResult *blockCompare)
{

	blockCompare->leftStart = m_leftFpr;
	blockCompare->rightStart = m_rightFpr;

	blockCompare->leftSize = m_leftContentLength;
	blockCompare->rightSize = m_rightContentLength;

	int m_lenA = blockCompare->leftSize;
	int m_lenB = blockCompare->rightSize;
	int blockSize = blockCompare->blockSize;

	int minLen = (m_lenA >= m_lenB) ? m_lenB : m_lenA;
	int times = minLen / blockSize;

	int curLeftOffset = 0;
	int curRightOffset = 0;


	QVector<BinUnequalPos>& leftUnequalPos = blockCompare->leftUnequalPos;
	QVector<BinUnequalPos>& rightUnequalPos = blockCompare->rightUnequalPos;
	QVector<BinUnequalPos>& leftSkipUnequalPos = blockCompare->leftSkipUnequalPos;
	QVector<BinUnequalPos>& rightSkipUnequalPos = blockCompare->rightSkipUnequalPos;;
	QString& leftPrintContents = blockCompare->leftPrintContents;
	QString& rightPrintContents = blockCompare->rightPrintContents;


	if (times > 0)
	{
		for (int i = 0; i < times; ++i)
		{
			outputBinBlock(blockCompare->lcsSize[i],blockSize, blockSize,curLeftOffset, curRightOffset, blockCompare->lcsDatas[i], m_leftFpr+i* blockSize, m_rightFpr + i * blockSize, \
						leftUnequalPos, rightUnequalPos, leftSkipUnequalPos, rightSkipUnequalPos, \
						leftPrintContents, rightPrintContents, curLeftOffset, curRightOffset);
		}
	}

	//尾巴上还有一次
	if ((minLen % blockSize) > 0)
	{
		int remainA = m_lenA - times * blockSize;
		int remainB = m_lenB - times * blockSize;
		int i = times;

		outputBinBlock(blockCompare->lcsSize[i], remainA, remainB, curLeftOffset, curRightOffset, blockCompare->lcsDatas[i], m_leftFpr + i * blockSize, m_rightFpr + i * blockSize, \
			leftUnequalPos, rightUnequalPos, leftSkipUnequalPos, rightSkipUnequalPos, \
			leftPrintContents, rightPrintContents, curLeftOffset, curRightOffset);
	}
}


//becmp的对比规律：
//1)一定是左右的行进行对比，即相同和不同都是在同一行之间进行体现，不可能斜跨多行

//发现这样的效果，没有becompore的好。
//虽然找到了最大lcs，但是相等的lcs不在同一行。而是垮了不同的行。
void CmpareMode::lineCmpLcs(const int lineNum, QVector<LineFileInfo> &linesInfo, QChar* lcsSrc, int lcsLens, QVector<TextCmpInfo> & result)
{
	if (lcsLens <= 0 || lcsSrc == nullptr)
	{
		return;
	}

	int fileSrcIndex = 0;
	int noEqualTimes = 0;
	int equalTimes = 0;
	int lcsSrcIndex = 0;


	QChar* start = (QChar*)linesInfo.at(lineNum).unicodeStr.data();
	int linesLength = linesInfo.at(lineNum).unicodeStr.count();

		//第二层循环，处理一行的每一个字符
		for (fileSrcIndex = 0; fileSrcIndex < linesLength;)
		{
			//第三层循环，对比每一个字符与lcs相等的关系。并把相等的和不相等的分开为一段段的TextCmpInfo，加入容器
			while (fileSrcIndex < linesLength && start[fileSrcIndex] != lcsSrc[lcsSrcIndex])
			{
				if (equalTimes > 0)
				{
					TextCmpInfo cmpinfo;
					cmpinfo.equal = true;
					cmpinfo.text.append(QString(start + fileSrcIndex - equalTimes, equalTimes));

					result.append(cmpinfo);
					equalTimes = 0;
				}
				++fileSrcIndex;
				++noEqualTimes;
			}

			//加入不相等的
			if (noEqualTimes > 0)
			{
				TextCmpInfo cmpinfo;
				cmpinfo.equal = false;
				cmpinfo.text.append(QString(start + fileSrcIndex - noEqualTimes, noEqualTimes));
				result.append(cmpinfo);
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
			TextCmpInfo cmpinfo;
			cmpinfo.equal = true;
			cmpinfo.text.append(QString(start + fileSrcIndex - equalTimes, equalTimes));
			result.append(cmpinfo);
			equalTimes = 0;
		}
		else if (noEqualTimes > 0)
		{
			TextCmpInfo cmpinfo;
			cmpinfo.equal = false;
			cmpinfo.text.append(QString(start + fileSrcIndex - noEqualTimes, noEqualTimes));
			result.append(cmpinfo);
			noEqualTimes = 0;
		}

}

//识别文字编码，并将文字按照原始编码格式，转换为QString。如果失败，默认按照utf8的格式进行转换；
bool CmpareMode::recognizeTextCode(QByteArray & text, LineFileInfo &lineInfo, QString &outUnicodeText)
{
	int lineNums = lineInfo.lineNums;

	int length = text.count();

	int result = false;

	//第一行时，检测一下文件编码，返回值也是文件的编码
	if (0 == lineNums)
	{
		int skip = 0;
		lineInfo.code = Encode::DetectEncode((uchar*) text.data(), length, skip);
		//根据编码跳过第一行前面的几个字符编码标识字段
		if (skip > 0)
		{
			text = text.mid(skip);
		}

		return Encode::tranStrToUNICODE((CODE_ID)lineInfo.code, text.data(), text.count(), outUnicodeText);

	}
	else
	{
		/*对于头部没有标识的行，需要每行进行详细检查，比较耗时
		*对于第一行已经是GBK的编码，标识出所有的确是GBK的行号
		*严格来说，如果以后要做国际版，不应该只考虑GBK，而是要考虑本地ASNI编码。
		*对中国而言，本地ASNI编码是GBK，对其它国家，比如日本/韩国而言，这些ASNI是它们本国
		*对应的本地编码。
		*/
//#if 0
//		//全部都在ascii范围以内，就作为ascii码。注意ASCII处理时其它地方时按照UTF8进行编码的
//		if (Encode::CheckTextIsAllAscii((uchar*)text.data(), length))
//		{
//			lineInfo.code = CODE_ID::ASCII;
//			return Encode::tranStrToUNICODE((CODE_ID)lineInfo.code, text.data(), length, outUnicodeText);
//		}
//		else
//		{
//#endif
			CODE_ID actualCode = Encode::CheckUnicodeWithoutBOM((uchar*)text.data(), length, outUnicodeText);
			if (CODE_ID::UTF8_NOBOM == actualCode)
			{
				lineInfo.code = CODE_ID::UTF8_NOBOM;
				result = true;
			}
			else if (CODE_ID::GBK == actualCode)
			{
				//如果发现存在GBK，则要以GBK作为字符编码。这里识别gbk是因为显示的时候，需要转化gbk进行显示
				lineInfo.code = CODE_ID::GBK;
				result = true;
			}
			else if (CODE_ID::ANSI == actualCode)
			{
				lineInfo.code = CODE_ID::UNKOWN; //这里就是乱码了。即不是utf8也不是GBK，也不能说乱码，目前其它国家未处理的码
				result = false;
			}
//#if 0
//		}
//#endif
	}

	return result;
}

//识别出文件前面带BOM的编码
CODE_ID CmpareMode::getTextFileEncodeType(RC_DIRECTION dir)
{
	CODE_ID code = CODE_ID::UNKOWN;

	CODE_ID & srcCode = (dir == RC_LEFT)? m_leftCode:m_rightCode;
	qint64& fileLength = ((dir == RC_LEFT) ? m_leftContentLength : m_rightContentLength);
	uchar* & fileFpr = ((dir == RC_LEFT) ? m_leftFpr : m_rightFpr);
	QString & filePath = ((dir == RC_LEFT) ? m_leftFilePath : m_rightFilePath);

		//如果外部强行指定编码，按照外面指定的来
	if (srcCode != UNKOWN)
		{
		return srcCode;
		}

	if (fileLength >= 2 && fileFpr[0] == 0xFF && fileFpr[1] == 0xFE)
		{
			return CODE_ID::UNICODE_LE; //skip 2
		}
	else if (fileLength >= 2 && fileFpr[0] == 0xFE && fileFpr[1] == 0xFF)
		{
			return CODE_ID::UNICODE_BE; //skip 2
		}
	else if (fileLength >= 3 && fileFpr[0] == 0xEF && fileFpr[1] == 0xBB && fileFpr[2] == 0xBF)
		{
			return CODE_ID::UTF8_BOM; //skip 3 with BOM
		}
		else
		{
		code = scanFileRealCode(filePath);
	}

	//发现有时检查unicode_le会失败，再粗略检查一下，是否是UNICODE_LE_NOBOM
	if (code == CODE_ID::UNKOWN && (fileLength > 2 && (fileLength % 2 == 0) && fileFpr[0] != 0 && fileFpr[1] == 0))
	{
		code = UNICODE_LE_NOBOM;
		}

	return code;
		}

//在批量转换编码中调用
CODE_ID CmpareMode::getTextFileEncodeType(QString filePath, int scanLineNum, QByteArray* contentbytes)
{

	CODE_ID code = CODE_ID::UNKOWN;

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly))
	{
		return code;
	}
	qint64 fileLength = file.size();

	if (fileLength <= 2)
	{
	file.close();
		return CODE_ID::UNKOWN;
	}

	QByteArray bytes = file.read(5);

	//外面可能还需要对文件进行二次检测，为加快速度，这里直接返回一部分文件内容
	if (contentbytes != nullptr)
	{
		qint64 MAX_LINE_LEN_TRY_READ = 1000 * 500;//读取500k的内容进行编码识别

		if (fileLength < MAX_LINE_LEN_TRY_READ)
		{
			MAX_LINE_LEN_TRY_READ = fileLength;
		}

		file.seek(0);
		*contentbytes = file.read(MAX_LINE_LEN_TRY_READ);
	}

	file.close();

	uchar* fileFpr = (uchar*)bytes.data();


	if (fileLength >= 2 && fileFpr[0] == 0xFF && fileFpr[1] == 0xFE)
	{
		return CODE_ID::UNICODE_LE; //skip 2
	}
	else if (fileLength >= 2 && fileFpr[0] == 0xFE && fileFpr[1] == 0xFF)
	{
		return CODE_ID::UNICODE_BE; //skip 2
	}
	else if (fileLength >= 3 && fileFpr[0] == 0xEF && fileFpr[1] == 0xBB && fileFpr[2] == 0xBF)
	{
		return CODE_ID::UTF8_BOM; //skip 3 with BOM
	}
	else
	{
		code = scanFileRealCode(filePath, scanLineNum);
	}

	//发现有时检查unicode_le会失败，再粗略检查一下，是否是UNICODE_LE_NOBOM
	if (code == CODE_ID::UNKOWN && (fileLength > 2 && (fileLength % 2 == 0) && fileFpr[0] != 0 && fileFpr[1] == 0))
	{
		code = UNICODE_LE_NOBOM;
	}

	return code;
}


//LE编码要特殊对待。
bool CmpareMode::isUnicodeLeBomFile(uchar* fileFpr, int fileLength)
{
	if (fileLength >= 2 && fileFpr[0] == 0xFF && fileFpr[1] == 0xFE)
	{
		return true;
	}
	return false;
}

//isCheckHead:是否检测头。只有文件开头，才有。如果是分块加载，中间打开的文件，则不需要检测。默认检测
//20230411 引入了开源编码识别库uchardet。
CODE_ID CmpareMode::getTextFileEncodeType(uchar* fileFpr, int fileLength, QString filePath, bool isCheckHead)
{
	if (isCheckHead)
	{
		if (fileLength >= 2 && fileFpr[0] == 0xFF && fileFpr[1] == 0xFE)
		{
		return CODE_ID::UNICODE_LE; //skip 2
	}
	else if (fileLength >= 2 && fileFpr[0] == 0xFE && fileFpr[1] == 0xFF)
	{
		return CODE_ID::UNICODE_BE; //skip 2
	}
	else if (fileLength >= 3 && fileFpr[0] == 0xEF && fileFpr[1] == 0xBB && fileFpr[2] == 0xBF)
	{
		return CODE_ID::UTF8_BOM; //skip 3 with BOM
	}
	}
	CODE_ID code = CODE_ID::UNKOWN;

	//走到这里说明没有文件头BOM，进行全盘文件扫描。发现uchardet里面也会判断头
	if (!filePath.isEmpty())
	{
		code = scanFileRealCode(filePath);
	}

	//发现有时检查unicode_le会失败，再粗略检查一下，是否是UNICODE_LE_NOBOM
	if (code == CODE_ID::UNKOWN && (fileLength > 2 && (fileLength % 2 == 0) && fileFpr[0] != 0 && fileFpr[1] == 0))
	{
		code = UNICODE_LE_NOBOM;
}

	return code;
}
//20210802：发现如果是CODE_ID::UNICODE_LE，\r\n变成了\r\0\n\0，读取readLine遇到\n就结束了，而且toUnicode也会变成乱码失败
//所以UNICODE_LE需要单独处理。该函数只处理Unicode_LE编码文件，事先一定要检查文件编码
//返回字符数，不是编码类型，注意是字符数，不是bytes
quint32 CmpareMode::readLineFromFileWithUnicodeLe(uchar* m_fileFpr, const int fileLength, QList<LineFileInfo>& lineInfoVec, QList<LineFileInfo>& blankLineInfoVec,int mode, int &maxLineSize)
{
	QCryptographicHash md4(QCryptographicHash::Md4);

	int lineNums = 0;
	CODE_ID code = CODE_ID::UNICODE_LE;

	int lineStartPos = 2; //uicode_le前面有2个特殊标识，故跳过2

	//获取一行在文件中
	auto getOneLineFromFile = [m_fileFpr](int& startPos, const int fileLength, QByteArray& ret)->bool{

		if (startPos < fileLength)
		{
			ret.clear();

			int lineLens = 0;

			bool isFindLine = false;

			for (int i = startPos; i < fileLength; ++i,++lineLens)
			{
				//遇到换行符号。20230722 吃了大亏，对比时解析错误。必须要后面是\0才行。之前没有检查
				//在LE编码下 殷契文渊http://jgw.aynu.edu.cn/ 会导致从文渊后面被截断。后续这里都要加上，不能存在侥幸心里。
				//导致一行被截断为两行，进而在对比过程中对齐失效。fuck,查找到晚上一点才找到问题。
				if (m_fileFpr[i] == 0x0A && ((i + 1 < fileLength) && m_fileFpr[i + 1] == 0x0))
				{
					if (startPos + lineLens + 2 < fileLength)
					{
					//lineLens需要加2，因为当前这个没有加，而且后面还有一个\0,这是le格式规定的
					ret.append((char*)(m_fileFpr + startPos), lineLens + 2);
					startPos += lineLens + 2;
					}
					else
					{
						//这里就是特殊情况，尾部后面没有\0，只能前进1个。
						//其实是容错，可能最后一个没有\0
						ret.append((char*)(m_fileFpr + startPos), lineLens + 1);
						//必须手动补上一个\0。避免格式残缺
						ret.append('\0');
						startPos += lineLens + 1;
					}
					isFindLine = true;
					break;
				}
			}

			//没有找到一行
			if (!isFindLine)
			{
				//最后一行，可能没有带\r\0直接返回
				ret.append((char*)(m_fileFpr + startPos), fileLength - startPos);

				startPos = fileLength;
			}

			return true;

		}

		return false;
	};

	QByteArray line;

	quint32 charNums = 0;

	auto work = [mode, &md4](LineFileInfo& lineInfo, const int n) {
		if (mode == 0)
		{
			md4.addData(lineInfo.unicodeStr.trimmed().toUtf8());
		}
		else if (mode == 1)
		{
			md4.addData(lineInfo.unicodeStr.left(lineInfo.unicodeStr.length() - n).toUtf8());
		}
		else if (mode == 2)
		{
			QString temp = lineInfo.unicodeStr;
			md4.addData(temp.replace(QRegExp("\\s"), QString("")).toUtf8());
		}
	};

	while (getOneLineFromFile(lineStartPos, fileLength,line)) {

		LineFileInfo lineInfo;

		lineInfo.lineNums = lineNums;

		/* 这种方式读取文件会包含后面的行尾 */
		int length = line.length();

		if (maxLineSize < length)
		{
			maxLineSize = length;
		}

		//如果是头部有标识的格式，则后续不用详细检查每行编码，直接按照头部标识走
		Encode::tranStrToUNICODE(code, line.data(), line.count(), lineInfo.unicodeStr);
		lineInfo.code = code;

		charNums += lineInfo.unicodeStr.size();

		if (lineInfo.unicodeStr.endsWith("\r\r\n"))
		{
			//这里是一种错误，但确实可能出现
			if (length > 3)
			{
				work(lineInfo, 3);
				}
			else
			{
				//空白行
				lineInfo.isLcsExist = false;
				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::DOS_LINE;
		}
		else if (lineInfo.unicodeStr.endsWith("\r\n"))
		{
			if (length > 2)
			{
				work(lineInfo, 2);
				}
			else
			{
				//空白行
				lineInfo.isLcsExist = false;
				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::DOS_LINE;

		}
		else if (lineInfo.unicodeStr.endsWith("\n"))
		{
			if (length > 1)
			{
				work(lineInfo, 1);
				}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::UNIX_LINE;

		}
		else if (lineInfo.unicodeStr.endsWith("\r"))
		{
			if (length > 1)
			{
				work(lineInfo, 1);
				}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::MAC_LINE;
		}
		else
		{
			if (length > 0)
			{
				work(lineInfo, 0);
				}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;

			}
			lineInfo.lineEndFormat = RC_LINE_FORM::UNKNOWN_LINE;
		}

		if (lineInfo.isEmptyLine)
		{
			blankLineInfoVec.append(lineInfo);
		}
		else
		{
			lineInfo.md4 = md4.result();
			//qDebug() << lineInfo.md4;
			md4.reset();
			lineInfoVec.append(lineInfo);
		}
		++lineNums;
	}

	return charNums;
}


//读取每一行，将空白行和非空白行分开。非空白行取他们的行md4值（不包含尾部的换行符）
//返回值：文件扫描出来的字符编码
//在对比行的md5值时，忽略了后面的行尾类型。即只对比字符内容，忽略了行尾。
//20210802：发现如果是CODE_ID::UNICODE_LE，\r\n变成了\r\0\n\0，读取readLine遇到\n就结束了，而且toUnicode也会变成乱码失败
//所以UNICODE_LE需要单独处理。注意UNICODE_BE没有这个问题，因为BE是\0\r\0\n，0在前面就没有这个问题

//20210901 发现使用readLine的方式来读取一行不可靠。因为有些文件中一行中间有个\r，这种情况没有识别为多行。readLine是根据\n来识别的。
//进而导致中间的\r没有识别为多行，但是在编辑器中却多一行，导致对比错误。还是要自己来识别行。不依赖于readLine


//CODE_ID fileCode 事先预判定的编码
CODE_ID CmpareMode::readLineFromFile(uchar* m_fileFpr, const int fileLength, const CODE_ID fileCode, QList<LineFileInfo>&lineInfoVec, QList<LineFileInfo>&blankLineInfoVec, int mode, int &maxLineSize)
{
	QCryptographicHash md4(QCryptographicHash::Md4);

	int lineNums = 0;
	CODE_ID code = fileCode;
	bool isExistGbk = false;
	bool isExistUnKownCode = false;
	bool isExistUtf8 = false;


	int lineStartPos = 0; 

	//跳过前面的BOM头部。LE不在这里处理，在外面
	if (fileCode == CODE_ID::UNICODE_BE || fileCode == CODE_ID::UNICODE_LE)
	{
		lineStartPos = 2;
	}
	else if (fileCode == CODE_ID::UTF8_BOM)
	{
		lineStartPos = 3;
	}

	//获取一行在文件中
	auto getOneLineFromFile = [m_fileFpr](int& startPos, const int fileLength, const CODE_ID fileCode, QByteArray& ret)->bool {

		if (startPos < fileLength)
		{
			ret.clear();

			int lineLens = 0;
			bool isFindLine = false;

			for (int i = startPos; i < fileLength; ++i, ++lineLens)
			{
				//遇到符号CR
				if (m_fileFpr[i] == 0x0D)
				{
					//后一个是LF,即以CRLF结尾
					if ((i + 1 < fileLength) && (m_fileFpr[i+1] == 0x0A))
					{
						//lineLens需要加2，因为当前这个没有加，而且后面还有一个\n
						ret.append((char*)(m_fileFpr + startPos), lineLens + 2);
						startPos += lineLens + 2;
						isFindLine = true;
						break;
					}
					else if ((fileCode == UNICODE_BE)&&((i>0) && m_fileFpr[i-1] == '\0'))
					{
						//事先发现就是BE格式，以\0\r\0\n为结尾的
						if ((i + 2 < fileLength) && (m_fileFpr[i + 1] == 0x0) && (m_fileFpr[i + 2] == 0x0A))
						{
							//lineLens需要加3，因为当前这个没有加，而且后面还有一个\0\n
							ret.append((char*)(m_fileFpr + startPos), lineLens + 3);
							startPos += lineLens + 3;
							isFindLine = true;
							break;
						}
						else 
						{
							//虽然说是BE格式，但是后面没有以\0\n结尾，而是以\r结尾。这种多半就是错误。直接按\0\r结尾
							//lineLens需要加1，因为当前这个没有加
							ret.append((char*)(m_fileFpr + startPos), lineLens + 1);
							startPos += lineLens + 1;
							isFindLine = true;
							break;
						}

					}
					else
					{
						//直接以\r结尾了，后面没有\n或者\0\n。符合MAC格式，windows可能编码只有\r，没有\n的错误情况。
						//lineLens需要加1，因为当前这个没有加
						ret.append((char*)(m_fileFpr + startPos), lineLens + 1);
						startPos += lineLens + 1;
						isFindLine = true;
						break;
					}
				}
				else if(m_fileFpr[i] == 0x0A)
				{
					//没有先遇到\r，直接遇到\n.20210903发现忘记处理该情况le
					//lineLens需要加1，因为当前这个没有加
					ret.append((char*)(m_fileFpr + startPos), lineLens + 1);
					startPos += lineLens + 1;
					isFindLine = true;
					break;
				}
			}

			//没有找到一行
			if (!isFindLine)
			{
				//最后一行，可能没有带\r\0直接返回
				ret.append((char*)(m_fileFpr + startPos), fileLength - startPos);

				startPos = fileLength;
			}

			return true;

		}

		return false;
	};

	QByteArray line;

	//返回：是否空白行。true是，false 不是空白行。
	//在下面根据Mode 如果是忽略空白，对于忽略后变成空行的进一步精确识别。
	auto work = [mode,&md4](LineFileInfo& lineInfo, const int n) {
		bool ret = false;
		QByteArray temp;
		if (mode == 0)
		{
			temp = lineInfo.unicodeStr.trimmed().toUtf8();
			ret = temp.isEmpty();
		}
		else if (mode == 1)
		{
			temp = lineInfo.unicodeStr.left(lineInfo.unicodeStr.length() - n).toUtf8();
			ret = temp.isEmpty();
		}
		else if (mode == 2)
		{
			QString str = lineInfo.unicodeStr;
			temp = str.replace(QRegExp("\\s"), QString("")).toUtf8();
			ret = temp.isEmpty();

			//20230419 这里对应的是忽略模式。忽略所有的空白字符。前面使用替换把所有空白都替换了。
			//如果替换后，发现该行是空行了，则把原始字符除\r\n以外的空白都替换为空。这里修改了原始内容。
			//因为发现如果不修改原始内容，外面也还是会对比原始内容，也不会认定该行是空行。
			//这样修改后，可以精确把\r\n 和 \t\r\n这种内容的差异行，认定为相等。如果不这样修改，这不能认定相等。
			//进一步提高了对比的精度。暂时不修改mode=0 和 1的场景，因为只有mode=2才是忽略模式，才可以这样忽略处理。
			if (ret)
			{
				lineInfo.unicodeStr.replace(QRegExp("[\\t ]"), QString(""));
		}
		}

		if (!ret)
		{
			md4.addData(temp);
		}
		else
		{
			//如果是空，做空行处理。
			lineInfo.isLcsExist = false;
			lineInfo.isEmptyLine = true;
		}
		return ret;
	};

	while (getOneLineFromFile(lineStartPos, fileLength, code, line)) {

		LineFileInfo lineInfo;

		lineInfo.lineNums = lineNums;

		/* 这种方式读取文件会包含后面的行尾 */
		int length = line.length();

		if (maxLineSize < length)
		{
			maxLineSize = length;
		}
		//外面必须把code先检测好了

		//if (fileCode == CODE_ID::UNICODE_BE /*|| fileCode == CODE_ID::UNICODE_LE */ || fileCode == CODE_ID::UTF8_BOM)
		if(fileCode != CODE_ID::UNKOWN)
		{
			//如果是头部有标识的格式，则后续不用详细检查每行编码，直接按照头部标识走
			Encode::tranStrToUNICODE(code, line.data(), line.count(), lineInfo.unicodeStr);
			lineInfo.code = fileCode;
		}
		else if(fileCode == CODE_ID::UNKOWN)
		{
			/*对于头部没有标识的行，需要每行进行详细检查，比较耗时
			*对于第一行已经是GBK的编码，标识出所有的确是GBK的行号
			*严格来说，如果以后要做国际版，不应该只考虑GBK，而是要考虑本地ASNI编码。
			*对中国而言，本地ASNI编码是GBK，对其它国家，比如日本/韩国而言，这些ASNI是它们本国
			*对应的本地编码。
			*/
			recognizeTextCode(line, lineInfo, lineInfo.unicodeStr);

			if (CODE_ID::UTF8_NOBOM == lineInfo.code)
			{
				isExistUtf8 = true;
			}
			else if (CODE_ID::GBK == lineInfo.code)
			{
				//如果发现存在GBK，则要以GBK作为字符编码。这里识别gbk是因为显示的时候，需要转化gbk进行显示
				isExistGbk = true;
			}
			else if (CODE_ID::UNKOWN == lineInfo.code)
			{
				isExistUnKownCode = true;
			}

		}

		if (lineInfo.unicodeStr.endsWith("\r\r\n"))
		{
			//这里是一种错误，但确实可能出现
			if (length > 3)
			{
				work(lineInfo,3);
			}
			else
			{
				//空白行
				lineInfo.isLcsExist = false;
				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::DOS_LINE;
		}
		else if (lineInfo.unicodeStr.endsWith("\r\n"))
		{
			if (length > 2)
			{
				work(lineInfo, 2);
			}
			else
			{
				//空白行
				lineInfo.isLcsExist = false;
				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::DOS_LINE;

		}
		else if (lineInfo.unicodeStr.endsWith("\n"))
		{
			if (length > 1)
			{
				work(lineInfo, 1);
			}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::UNIX_LINE;

		}
		else if (lineInfo.unicodeStr.endsWith("\r"))
		{
			if (length > 1)
			{
				work(lineInfo, 1);
			}
			else
			{
				lineInfo.isLcsExist = false;
				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::MAC_LINE;
		}
		else
		{
			if (length > 0)
			{
				work(lineInfo, 0);
			}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;

			}
			lineInfo.lineEndFormat = RC_LINE_FORM::UNKNOWN_LINE;
		}

		if (lineInfo.isEmptyLine)
		{
			blankLineInfoVec.append(lineInfo);
		}
		else
		{
			lineInfo.md4 = md4.result();
			md4.reset();
			lineInfoVec.append(lineInfo);
		}
		++lineNums;
	}

	//如果外部指定了格式，则直接返回外部格式
	if (fileCode != CODE_ID::UNKOWN)
	{
		return fileCode;
	}

	return judgeFinalTextCode(code, isExistUnKownCode, isExistGbk, isExistUtf8);
}

CODE_ID CmpareMode::judgeFinalTextCode(CODE_ID code, bool isExistUnKownCode, bool isExistGbk, bool isExistUtf8)
{
	//如果是三种有明确标识的字符编码，则严格按照标识的逻辑去读取。哪怕里面存在错误编码，也只能按照头部标识为准
	if (CODE_ID::UNICODE_LE == code || CODE_ID::UNICODE_BE == code || CODE_ID::UTF8_BOM == code || code == CODE_ID::GBK)
	{
		return code;
	}

	//剩下的是在文件头没有严格标识编码的文件
	//存在不能识别的编码，则应该是ASNI，需要用户指定编码
	if (isExistUnKownCode)
	{
		return CODE_ID::UNKOWN;
	}
	if (isExistGbk)
	{
		//如果没有错误码，而且发现gbk，则是gbk编码
		return CODE_ID::GBK;
	}
	//如果不存在错误和gbk，就是纯粹的ut8_nobom
	if (isExistUtf8)
	{
		return CODE_ID::UTF8_NOBOM;
	}

	return code;
}


//读取用于纯输出，不做比较。bool &isMaybeHexFile 是否是hex文件，不一定准确，做一个推测
// int& charsNums 输出字符个数
CODE_ID CmpareMode::readLineFromFile(uchar* m_fileFpr, const int fileLength, const CODE_ID fileCode, QList<LineFileInfo>&lineInfoVec, int& maxLineSize, int& charsNums, bool &isMaybeHexFile)
{
	int lineNums = 0;
	CODE_ID code = fileCode;
	bool isExistGbk = false;
	bool isExistUnKownCode = false;
	bool isExistUtf8 = false;
	int lineStartPos = 0;
	int errorCodeLines = 0;
	charsNums = 0;

	if (fileCode == CODE_ID::UNICODE_BE || fileCode == CODE_ID::UNICODE_LE)
	{
		lineStartPos = 2;
	}
	else if (fileCode == CODE_ID::UTF8_BOM)
	{
		lineStartPos = 3;
	}

	//获取一行在文件中
	auto getOneLineFromFile = [m_fileFpr](int& startPos, const int fileLength, const CODE_ID fileCode, QByteArray& ret)->bool {

		if (startPos < fileLength)
		{
			ret.clear();

			int lineLens = 0;
			bool isFindLine = false;

			for (int i = startPos; i < fileLength; ++i, ++lineLens)
			{
				//遇到符号CR
				if (m_fileFpr[i] == 0x0D)
				{
					//后一个是LF,即以CRLF结尾
					if ((i + 1 < fileLength) && (m_fileFpr[i + 1] == 0x0A))
					{
						//lineLens需要加2，因为当前这个没有加，而且后面还有一个\n
						ret.append((char*)(m_fileFpr + startPos), lineLens + 2);
						startPos += lineLens + 2;
						isFindLine = true;
						break;
					}
					else if ((fileCode == UNICODE_BE) && ((i > 0) && m_fileFpr[i - 1] == '\0'))
					{
						//事先发现就是BE格式，以\0\r\0\n为结尾的
						if ((i + 2 < fileLength) && (m_fileFpr[i + 1] == 0x0) && (m_fileFpr[i + 2] == 0x0A))
						{
							//lineLens需要加3，因为当前这个没有加，而且后面还有一个\0\n
							ret.append((char*)(m_fileFpr + startPos), lineLens + 3);
							startPos += lineLens + 3;
							isFindLine = true;
							break;
						}
						else
						{
							//虽然说是BE格式，但是后面没有以\0\n结尾，而是以\r结尾。这种多半就是错误。直接按\0\r结尾
							//lineLens需要加1，因为当前这个没有加
							ret.append((char*)(m_fileFpr + startPos), lineLens + 1);
							startPos += lineLens + 1;
							isFindLine = true;
							break;
						}

					}
					else
					{
						//直接以\r结尾了，后面没有\n或者\0\n。符合MAC格式，windows可能编码只有\r，没有\n的错误情况。
						//lineLens需要加1，因为当前这个没有加
						ret.append((char*)(m_fileFpr + startPos), lineLens + 1);
						startPos += lineLens + 1;
						isFindLine = true;
						break;
					}
				}
				else if (m_fileFpr[i] == 0x0A)
				{
					//没有先遇到\r，直接遇到\n.20210903发现忘记处理该情况le
					//lineLens需要加1，因为当前这个没有加
					ret.append((char*)(m_fileFpr + startPos), lineLens + 1);
					startPos += lineLens + 1;
					isFindLine = true;
					break;
				}
			}

			//没有找到一行
			if (!isFindLine)
			{
				//最后一行，可能没有带\r\0直接返回
				ret.append((char*)(m_fileFpr + startPos), fileLength - startPos);

				startPos = fileLength;
			}

			return true;

		}

		return false;
	};

	QByteArray line;


	while (getOneLineFromFile(lineStartPos, fileLength, code, line)) {

		LineFileInfo lineInfo;

		lineInfo.lineNums = lineNums;

		/* 这种方式读取文件会包含后面的行尾 */
		int length = line.length();

		if (maxLineSize < length)
		{
			maxLineSize = length;
		}

		//外面必须把code先检测好了

		//if (fileCode == CODE_ID::UNICODE_BE /*|| fileCode == CODE_ID::UNICODE_LE */ || fileCode == CODE_ID::UTF8_BOM)
		if(fileCode != CODE_ID::UNKOWN)
		{
			//如果是头部有标识的格式，则后续不用详细检查每行编码，直接按照头部标识走
			Encode::tranStrToUNICODE(code, line.data(), line.count(), lineInfo.unicodeStr);
			lineInfo.code = fileCode;
		}
		else if (fileCode == CODE_ID::UNKOWN)
		{
			/*对于头部没有标识的行，需要每行进行详细检查，比较耗时
			*对于第一行已经是GBK的编码，标识出所有的确是GBK的行号
			*严格来说，如果以后要做国际版，不应该只考虑GBK，而是要考虑本地ASNI编码。
			*对中国而言，本地ASNI编码是GBK，对其它国家，比如日本/韩国而言，这些ASNI是它们本国
			*对应的本地编码。
			*/
			recognizeTextCode(line, lineInfo, lineInfo.unicodeStr);

			if (CODE_ID::UTF8_NOBOM == lineInfo.code)
			{
				isExistUtf8 = true;
			}
			else if (CODE_ID::GBK == lineInfo.code)
			{
				//如果发现存在GBK，则要以GBK作为字符编码。这里识别gbk是因为显示的时候，需要转化gbk进行显示
				isExistGbk = true;
			}
			else if (CODE_ID::UNKOWN == lineInfo.code)
			{
				isExistUnKownCode = true;

				//增加错误行的计数
				errorCodeLines++;
			}

		}

		if (lineInfo.unicodeStr.endsWith("\r\r\n"))
		{
			//这里是一种错误，但确实可能出现
			if (length > 3)
			{

			}
			else
			{
				//空白行
				lineInfo.isLcsExist = false;
				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::DOS_LINE;
		}
		else if (lineInfo.unicodeStr.endsWith("\r\n"))
		{
			if (length > 2)
			{

			}
			else
			{
				//空白行
				lineInfo.isLcsExist = false;
				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::DOS_LINE;

		}
		else if (lineInfo.unicodeStr.endsWith("\n"))
		{
			if (length > 1)
			{

			}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::UNIX_LINE;

		}
		else if (lineInfo.unicodeStr.endsWith("\r"))
		{
			if (length > 1)
			{

			}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;
			}
			lineInfo.lineEndFormat = RC_LINE_FORM::MAC_LINE;
		}
		else
		{
			if (length > 0)
			{

			}
			else
			{
				lineInfo.isLcsExist = false;

				lineInfo.isEmptyLine = true;

			}
			lineInfo.lineEndFormat = RC_LINE_FORM::UNKNOWN_LINE;
		}

		lineInfoVec.append(lineInfo);

		charsNums += lineInfo.unicodeStr.size();
		++lineNums;
	}

	//如果超过一半的行都是错误的，则考虑为hex文件。
	if (lineNums >= 10 && (errorCodeLines * 100 / lineNums > 50))
	{
		isMaybeHexFile = true;
	}
	else
	{
		isMaybeHexFile = false;

		//如果前面三行中含有\0字符，也可能是二进制文件
		if (lineNums > 3)
		{
			for (int i = 0; i < 3; ++i)
			{
				if (lineInfoVec.at(i).unicodeStr.contains(QChar('\0')))
				{
					isMaybeHexFile = true;
					break;
				}
			}
		}
	}

	//如果用户外部强制编码，则直接按改编码返回
	if (fileCode != CODE_ID::UNKOWN)
	{
		return fileCode;
	}

	return judgeFinalTextCode(code, isExistUnKownCode, isExistGbk, isExistUtf8);
}


void CmpareMode::getCodeSkip(CODE_ID &leftCode, CODE_ID &rightCode, int &leftSkip, int &rightSkip)
{
	//检测文件字符编码

	leftCode = m_leftCode;
	rightCode = m_rightCode;
	leftSkip = m_leftSkip;
	rightSkip = m_rightSkip;

}

//扫描文件的字符编码，不输出文件
//扫描多少行scanLineNum 默认100
//如果是-1 之前全部扫描
////20230411 引入了开源编码识别库uchardet。真正的代码库路径：https://gitlab.freedesktop.org/uchardet/uchardet/-/tree/master/
//之前找了一个github上过期的代码uchardet库，导致没有最新的功能全。
CODE_ID CmpareMode::scanFileRealCode(QString filePath, int scanLineNum)
{
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly))
	{
		return CODE_ID::UNKOWN;
	}

	CODE_ID code = CODE_ID::UNKOWN;
	qint64 MAX_LINE_LEN_TRY_READ = 1000 * 10;//读取10k的内容进行编码识别

	//-1则之前是文件全部扫描，现在是500k
	if (scanLineNum == -1)
	{
		MAX_LINE_LEN_TRY_READ = 1000 * 500;
}

	QByteArray line = file.read(MAX_LINE_LEN_TRY_READ);
	file.close();

	uchardet_t ud = uchardet_new();

	int ret = uchardet_handle_data(ud, line.data(), line.size());
	uchardet_data_end(ud);

	if (ret == 0)
	{
		const char* codename = uchardet_get_charset(ud);
		if (strlen(codename) == 0)
		{
			//未知编码
		}

#if defined (Q_OS_WIN)
		else if ((stricmp(codename,"UTF-8") == 0)|| (stricmp(codename, "ASCII") == 0))
		{
			code = UTF8_NOBOM;
		}
		else if ((stricmp(codename, "GB18030") == 0) || (strcmp(codename, "HZ-GB-2312") == 0))
		{
			code = GBK;
		}
		else if (stricmp(codename, "BIG5") == 0)
		{
			code = BIG5;
		}
		else if (stricmp(codename, "UTF-16BE") == 0)
		{
			code = UNICODE_BE;
		}
		else if (stricmp(codename, "UTF-16LE") == 0)
		{
			code = UNICODE_LE;
		}
		else if (strnicmp(codename, "WINDOWS-1250",11) == 0) //1250 1252 125x都按照1250处理
		{
			code = WINDOWS1250;
		}
		else if (stricmp(codename, "SHIFT_JIS") == 0)
		{
			code = Shift_JIS;
		}
		else if (stricmp(codename, "IBM865") == 0 || stricmp(codename, "IBM866") == 0)
		{
			code = IBM866;
	}
		else if (stricmp(codename, "KOI8-R") == 0)
		{
			code = KOI8_R;
	}
		else if (stricmp(codename, "EUC-KR") == 0)
		{
			code = EUC_KR;
		}
#else
        else if ((strcasecmp(codename,"UTF-8") == 0)|| (strcasecmp(codename, "ASCII") == 0))
        {
            code = UTF8_NOBOM;
	}
        else if ((strcasecmp(codename, "GB18030") == 0) || (strcasecmp(codename, "HZ-GB-2312") == 0))
        {
            code = GBK;
        }
        else if (strcasecmp(codename, "BIG5") == 0)
        {
            code = BIG5;
        }
        else if (strcasecmp(codename, "UTF-16BE") == 0)
        {
            code = UNICODE_BE;
        }
        else if (strcasecmp(codename, "UTF-16LE") == 0)
        {
            code = UNICODE_LE;
        }
        else if (/*strnicmp*/ strncasecmp(codename, "WINDOWS-1250",11) == 0) //1250 1252 125x都按照1250处理
        {
            code = WINDOWS1250;
        }
        else if (strcasecmp(codename, "SHIFT_JIS") == 0)
        {
            code = Shift_JIS;
        }
        else if (strcasecmp(codename, "IBM865") == 0 || strcasecmp(codename, "IBM866") == 0)
        {
            code = IBM866;
        }
        else if (strcasecmp(codename, "KOI8-R") == 0)
        {
            code = KOI8_R;
        }
        else if (strcasecmp(codename, "EUC-KR") == 0)
        {
            code = EUC_KR;
        }
#endif

	}

	uchardet_delete(ud);

	return code;
}

//读取文件，并输出
//bytescharsNums:文件字符个数，不是文件大小
//20220908 自动判断是否是二进制文件。isHexFile 是输出
CODE_ID CmpareMode::scanFileOutPut(CODE_ID code, QString filePath, QList<LineFileInfo>& outputLineInfoVec, int &maxLineSize, int& charsNums, bool &isHexFile)
{
	QFile* file = new QFile(filePath);
	bool ok = file->open(QIODevice::ReadOnly);
	if (!ok)
	{
		return CODE_ID::UNKOWN;
	}
	uchar* m_fileFpr = file->map(0, file->size());

	if (code == UNKOWN)
	{
		code = getTextFileEncodeType(m_fileFpr, file->size(), filePath);
	}

	//UNICODE_LE格式需要单独处理
	if (code == UNICODE_LE)
	{
		charsNums = readLineFromFileWithUnicodeLe(m_fileFpr, file->size(), outputLineInfoVec, outputLineInfoVec, 0, maxLineSize);
	}
	else
	{
		code = readLineFromFile(m_fileFpr, file->size(), code, outputLineInfoVec, maxLineSize, charsNums, isHexFile);
	}

	file->unmap(m_fileFpr);
	file->close();
	delete file;

	return code;
}

//预先扫描文件，生成各自的行信息
int CmpareMode::preScanFile(AbstractCompare *blockCompare)
{
	CODE_ID code;

	//如果有一个文件的长度是空，则返回空
	//文件模式则要判断文件的大小
	if (m_leftFile != nullptr && m_rightFile != nullptr)
	{
		int emptyFileNum = 0;

		if (m_leftContentLength == 0 )
		{
			emptyFileNum++;
		}
		if (m_rightContentLength == 0)
		{
			emptyFileNum++;
		}

		if (emptyFileNum > 0)
		{
			blockCompare->setEmptyFileStatus(emptyFileNum);
			return EMPTY_FILE;
		}
		//UNICODE_LE格式需要单独处理
		code = getTextFileEncodeType(RC_LEFT);
	}
	else
	{
		//内存模式下，直接使用UTF8
		code = UTF8_NOBOM;
	}

	int maxLineSize = 0;

	//20240126 发现m_leftFpr可能为空导致崩溃
	if (m_leftFpr != 0)
	{
	if (code == UNICODE_LE)
	{
		m_leftCode = UNICODE_LE;
		readLineFromFileWithUnicodeLe(m_leftFpr, m_leftContentLength,m_leftLineInfoVec, m_leftBlankLineInfoVec, m_mode, maxLineSize);
	}
	else
	{
		m_leftCode = readLineFromFile(m_leftFpr, m_leftContentLength,code, m_leftLineInfoVec, m_leftBlankLineInfoVec,m_mode, maxLineSize);
	}
	}

	if (m_leftFile != nullptr && m_rightFile != nullptr)
	{
		//UNICODE_LE格式需要单独处理
		code = getTextFileEncodeType(RC_RIGHT);
	}
	else
	{
		//内存模式下，直接使用UTF8
		code = UTF8_NOBOM;
	}

	if (m_rightFpr != nullptr)
	{
	if (code == UNICODE_LE)
	{
		m_rightCode = UNICODE_LE;
		readLineFromFileWithUnicodeLe(m_rightFpr, m_rightContentLength, m_rightLineInfoVec, m_rightBlankLineInfoVec, m_mode, maxLineSize);
	}
	else
	{
		m_rightCode = readLineFromFile(m_rightFpr, m_rightContentLength, code, m_rightLineInfoVec, m_rightBlankLineInfoVec, m_mode, maxLineSize);
	}
	}

	//如果文件大小虽然不是0，但是时间内容行数是0，也不比较，发现如果直接比较会导致LCS里面死循环无法退出
	m_leftLinesNums = m_leftLineInfoVec.size() + m_leftBlankLineInfoVec.size();
	m_rightLinesNums = m_rightLineInfoVec.size() + m_rightBlankLineInfoVec.size();

	int emptyFileNum = 0;

	if (m_leftLinesNums == 0)
	{
		emptyFileNum++;
	}
	if (m_rightLinesNums == 0)
	{
		emptyFileNum++;
	}

	if (m_leftLinesNums > 3000 && m_rightLinesNums > 3000)
	{
		emit outputMsg(1,tr("BigFile Compare, left linenum %1 , right lineNum %2, Please Waiting !").arg(m_leftLinesNums).arg(m_rightLinesNums));
	}

	if (emptyFileNum > 0)
	{
		blockCompare->setEmptyFileStatus(emptyFileNum);
		return EMPTY_FILE;
	}

	return SCAN_SUCCESS;
}

//比较内容与LcsLine的差距
void CmpareMode::cmpDateAndLcsLine(QList<LineFileInfo>&lineInfoVec, int offset, QByteArray * lcsLine, int lcsLens, RC_DIRECTION direction, QMap<int, EqualLineInfo>& enqualLineInfoVec)
{
	int lineCount = lineInfoVec.size();

	int lineNum = offset;

	//这里说明LCS为0，即没有一行是相同的，全部不同
	if (0 == lcsLens)
	{
		return;
	}

	//比较非空白行中的每一行，与lcs最大公共子串的关系
	for (int cmpSrcIndex = 0; cmpSrcIndex < lcsLens; ++cmpSrcIndex)
	{
		//一直找到第一个出现在lcs中的行
		while (lineNum < lineCount && lineInfoVec[lineNum].md4 != lcsLine[cmpSrcIndex])
		{
			lineInfoVec[lineNum].isLcsExist = false;
			++lineNum;
		}

		//遇到一个相等的了
		lineInfoVec[lineNum].isLcsExist = true;

		//将相等的信息保存起来。相等序号-对应左边的行-对应右边的行
		if (!enqualLineInfoVec.contains(cmpSrcIndex))
		{
			//不包含则新建节点
			EqualLineInfo equalInfo;
			equalInfo.index = cmpSrcIndex;
			if (direction == RC_LEFT)
			{
				equalInfo.leftLineNums = lineInfoVec[lineNum].lineNums;
				equalInfo.rightLineNums = 0;
			}
			else
			{
				equalInfo.rightLineNums = lineInfoVec[lineNum].lineNums;
				equalInfo.leftLineNums = 0;
			}
			enqualLineInfoVec[cmpSrcIndex] = equalInfo;
		}
		else
		{
			//存在则更新
			if (direction == RC_LEFT)
			{
				enqualLineInfoVec[cmpSrcIndex].leftLineNums = lineInfoVec[lineNum].lineNums;
			}
			else
			{
				enqualLineInfoVec[cmpSrcIndex].rightLineNums = lineInfoVec[lineNum].lineNums;
			}
		}

		//遇到一个相等的了
		++lineNum;

		//这里其实算有错误
		assert(lineNum <= lineCount);
	}

}

//将左右内容与LCS行进行比较，相同的标记true，否则标记false
//2020-4-16 空白行中，其实还是有相同的块，即块号相同，而且都是空白行
void CmpareMode::cmpLcsLine(QByteArray * lcsLine, int lcsLens)
{
	cmpDateAndLcsLine(m_leftLineInfoVec, 0,lcsLine, lcsLens, RC_LEFT, m_enqualLineInfoVec);
	cmpDateAndLcsLine(m_rightLineInfoVec, 0, lcsLine, lcsLens, RC_RIGHT, m_enqualLineInfoVec);
}

QList<LineFileInfo>* CmpareMode::getLeftLineInfo()
{
	return &m_leftLineInfoVec;
}

QList<LineFileInfo>* CmpareMode::getRightLineInfo()
{
	return &m_rightLineInfoVec;
}

//QVector<LineFileInfo>* CmpareMode::getLeftBlankLineInfo()
//{
//	return &m_leftBlankLineInfoVec;
//}
//
//QVector<LineFileInfo>* CmpareMode::getRightBlankLineInfo()
//{
//	return &m_rightBlankLineInfoVec;
//}

void CmpareMode::getLines(RC_DIRECTION direction, QList<LineFileInfo>& lines)
{
	if (RC_LEFT == direction)
	{
		int count = m_leftLineInfoVec.count();
		int blankCount = m_leftBlankLineInfoVec.count();

		//有空白行，则要输出空白行
		int curLcsIndex = 0;
		int curBlankIndex = 0;

		int totolLine = count + blankCount;

		for (int i = 0; i < totolLine; ++i)
		{
			if (curLcsIndex < count && i == m_leftLineInfoVec[curLcsIndex].lineNums)
			{
				lines.append(m_leftLineInfoVec.at(curLcsIndex));
				++curLcsIndex;
			}
			else if (curBlankIndex < blankCount && i == m_leftBlankLineInfoVec[curBlankIndex].lineNums)
			{
				lines.append(m_leftBlankLineInfoVec.at(curBlankIndex));
				++curBlankIndex;
			}
		}
	}
	else
	{
		int count = m_rightLineInfoVec.count();
		int blankCount = m_rightBlankLineInfoVec.count();

		//有空白行，则要输出空白行
		int curLcsIndex = 0;
		int curBlankIndex = 0;

		int totolLine = count + blankCount;


		for (int i = 0; i < totolLine; ++i)
		{
			if (curLcsIndex < count && i == m_rightLineInfoVec[curLcsIndex].lineNums)
			{
				lines.append(m_rightLineInfoVec.at(curLcsIndex));
				++curLcsIndex;
			}
			else if (curBlankIndex < blankCount && i == m_rightBlankLineInfoVec[curBlankIndex].lineNums)
			{
				lines.append(m_rightBlankLineInfoVec.at(curBlankIndex));
				++curBlankIndex;
			}
		}
	}
}

//遍历每一个相等的行，将相等行合并为相等块。
//块即是 多个相等的行如果连续在一起，则合并为一个大的块
void CmpareMode::mergeBlockFromLine()
{
	if (m_enqualLineInfoVec.size() <= 0)
	{
		return;
	}

	QMap<int, EqualLineInfo>::iterator iter = m_enqualLineInfoVec.begin();

	int preLeft = (*iter).leftLineNums;
	int preRight = (*iter).rightLineNums;

	++iter;

	int curLeft = 0;
	int curRight = 0;

	int mergeTimes = 0;

	//注意上面已经跳过第一个了
	for (; iter != m_enqualLineInfoVec.end(); ++iter)
	{
		curLeft = (*iter).leftLineNums;
		curRight = (*iter).rightLineNums;

		//相当的行的前一行也相同，即连续相同则合并为一个块
		if ((preLeft == (curLeft - 1)) && (preRight == (curRight - 1)))
		{
			++mergeTimes;
		}
		else
		{
			if (mergeTimes > 0)
			{
				BlocksInfo leftEqualBlock(true, preLeft- mergeTimes, preLeft+1, mergeTimes);
				m_leftEqualBlockInfoVec.append(leftEqualBlock);

				BlocksInfo rightEqualBlock(true, preRight- mergeTimes, preRight + 1, mergeTimes);
				m_rightEqualBlockInfoVec.append(rightEqualBlock);

				mergeTimes = 0;
			}
			else
			{
				//前后不等，前面的单独一个块
				BlocksInfo leftEqualBlock(true, preLeft , preLeft + 1, 1);
				m_leftEqualBlockInfoVec.append(leftEqualBlock);

				BlocksInfo rightEqualBlock(true, preRight, preRight + 1, 1);
				m_rightEqualBlockInfoVec.append(rightEqualBlock);
			}


		}

		preLeft = curLeft;
		preRight = curRight;
	}
	//退出循环时，处理可能没有处理完毕的

	if (mergeTimes > 0)
	{
		BlocksInfo leftEqualBlock(true, preLeft- mergeTimes, preLeft + 1, mergeTimes);
		m_leftEqualBlockInfoVec.append(leftEqualBlock);

		BlocksInfo rightEqualBlock(true, preRight - mergeTimes, preRight + 1, mergeTimes);
		m_rightEqualBlockInfoVec.append(rightEqualBlock);

		mergeTimes = 0;
	}
	else
	{
		//前后不等，前面的单独一个块
		BlocksInfo leftEqualBlock(true, preLeft, preLeft + 1, 1);
		m_leftEqualBlockInfoVec.append(leftEqualBlock);

		BlocksInfo rightEqualBlock(true, preRight, preRight + 1, 1);
		m_rightEqualBlockInfoVec.append(rightEqualBlock);
	}
}

void CmpareMode::cmpAllBlockInfo()
{
	//先得到相等的块
	mergeBlockFromLine();

	//现在直接根据相等的块，推演出不相等的块
	//注意这样反对出来的不相等块，其实包含了之前说的空白行。这是之前没有考虑到的。所以空白索引行可能不需要

	if (m_leftEqualBlockInfoVec.size() == 0)
	{
		//没有相等的块，则全部是不相等的
		BlocksInfo leftNoEqualBlock(false, 0, m_leftLinesNums, m_leftLinesNums);
		m_leftNoEqualBlockInfoVec.append(leftNoEqualBlock);

		BlocksInfo rightNoEqualBlock(false, 0, m_rightLinesNums, m_rightLinesNums);
		m_rightNoEqualBlockInfoVec.append(rightNoEqualBlock);

	}
	else if (m_leftEqualBlockInfoVec.size() == m_leftLinesNums)
	{
		//全是相等的块，没有不相等的
	}
	else
	{
		//部分相等、部分不等

		QList<BlocksInfo>::iterator iterLeft = m_leftEqualBlockInfoVec.begin();
		int preLeft = 0;


		BlocksInfo leftNoEqualFirstBlock(false, 0, (*iterLeft).startLine, (*iterLeft).startLine);
		m_leftNoEqualBlockInfoVec.append(leftNoEqualFirstBlock);


		preLeft = iterLeft->endLine;
		++iterLeft;

		//反推出不相同的块
		for (; iterLeft != m_leftEqualBlockInfoVec.end(); ++iterLeft)
		{
			BlocksInfo leftNoEqualBlock(false, preLeft, (*iterLeft).startLine, (*iterLeft).startLine - preLeft);
			m_leftNoEqualBlockInfoVec.append(leftNoEqualBlock);
			preLeft = iterLeft->endLine;
		}

		//尾部最后的一块
		BlocksInfo leftNoEqualLastBlock(false, preLeft, m_leftLinesNums, m_leftLinesNums - preLeft);
		m_leftNoEqualBlockInfoVec.append(leftNoEqualLastBlock);

		//右边
		QList<BlocksInfo>::iterator iterRight = m_rightEqualBlockInfoVec.begin();
		int preRight = 0;


		BlocksInfo rightNoEqualFirstBlock(false, 0, (*iterRight).startLine, (*iterRight).startLine);
		m_rightNoEqualBlockInfoVec.append(rightNoEqualFirstBlock);


		preRight = iterRight->endLine;
		++iterRight;

		for (; iterRight != m_rightEqualBlockInfoVec.end(); ++iterRight)
		{
			BlocksInfo rightNoEqualBlock(false, preRight, (*iterRight).startLine, (*iterRight).startLine - preRight);
			m_rightNoEqualBlockInfoVec.append(rightNoEqualBlock);
			preRight = iterRight->endLine;
		}

		//尾部最后的一块
		BlocksInfo rightNoEqualLastBlock(false, preRight, m_rightLinesNums, m_rightLinesNums - preRight);
		m_rightNoEqualBlockInfoVec.append(rightNoEqualLastBlock);

	}

	//qDebug("--%d %d %d %d",gt1,gt2,gt3,gt4);
}


/* 该函数通过参数返回是一个指针的引用。而内容本身不需要在外面释放，因为m_leftEqualBlockInfoVec是栈变量 */
void CmpareMode::getBlockInfo(int direcion, QList<BlocksInfo> * &equalList, QList<BlocksInfo> * &unEqualList, uchar * &filePtr)
{
	if (direcion == LEFT)
	{
		equalList = &m_leftEqualBlockInfoVec;
		unEqualList = &m_leftNoEqualBlockInfoVec;
		filePtr = m_leftFpr;
	}
	else
	{
		equalList = &m_rightEqualBlockInfoVec;
		unEqualList = &m_rightNoEqualBlockInfoVec;
		filePtr = m_rightFpr;
	}
}


//注意外面要释放返回值
void CmpareMode::cmp(BinLcsResult *blockCompare)
{
	uchar* leftStart = m_leftFpr;
	uchar* rightStart = m_rightFpr;

	qint64 leftSize = m_leftContentLength;
	qint64 rightSize = m_rightContentLength;

	Lcs lcs(leftStart, rightStart, leftSize, rightSize);
	connect(&lcs, &Lcs::reportStep, this, &CmpareMode::outputMsg);
	lcs.setParameter(blockCompare);

	//小于16k的，直接暴力全部对比
	if (leftSize < 16384 && rightSize < 16384)
	{
		blockCompare->isBlockCmp = false;
		blockCompare->lcsContents = lcs.cmp(blockCompare->lcsLength);
}
	else
	{
		// 必须分块对比，每次8K，否则求解时间太长
		blockCompare->isBlockCmp = true;
		blockCompare->blockSize = 8192;

		lcs.cmp(8192, blockCompare->lcsDatas, blockCompare->lcsSize);
	}
}

static const qint64 MAX_CMP_LIMIT = 10000 * 10000;//每行最大1万个字符。否则不对比

//判断两个文件，在行的层面是否相等
//1 解析行，如果行的数据不一样长，则认定为不等。
//2 如果行数量一样长，则再解析lcs是否一样长。
bool CmpareMode::isEqualByLine(AbstractCompare* blockCompare)
{
	if (m_leftLineInfoVec.size() != m_rightLineInfoVec.size())
	{
		//如果行数量不一样长，则认定为不等拉倒。加快速度
		return false;
	}

	//提前判断。因为后面getLcsLength里面也需要判断，避免做无用功。

	qint64 totalSize = m_leftLineInfoVec.size() * m_rightLineInfoVec.size();

	if ((totalSize > MAX_CMP_LIMIT) || (totalSize < 0))
	{
		return false;
	}


	//下面就慢了，要按照行的层面去对比LCS。

	QByteArray* pLineLeft = new QByteArray[m_leftLineInfoVec.size()];

	int count = m_leftLineInfoVec.size();

	for (int i = 0; i < count; ++i)
	{
		pLineLeft[i] = m_leftLineInfoVec[i].md4;
	}


	QByteArray* pLineRight = new QByteArray[m_rightLineInfoVec.size()];

	count = m_rightLineInfoVec.size();

	for (int i = 0; i < count; ++i)
	{
		pLineRight[i] = m_rightLineInfoVec[i].md4;
	}

	//每一行的md4值组成一个数组，做一个LCS计算
	LcsLine lcs(pLineLeft, pLineRight, m_leftLineInfoVec.size(), m_rightLineInfoVec.size());
	lcs.setCancelFlag(blockCompare->m_isCancel);
	connect(&lcs, &LcsLine::outMsg, this, &CmpareMode::outputMsg);

	if (m_lcsLine != nullptr)
	{
		delete[] m_lcsLine;
		m_lcsLine = nullptr;
	}

	int lcsLens = lcs.getLcsLength(m_leftLineInfoVec.size(), m_rightLineInfoVec.size(), pLineLeft, pLineRight);

	delete[] pLineLeft;
	delete[] pLineRight;

	//如果lcs长度和原始行一样长，则认定为相等。
	return (m_leftLineInfoVec.size() == lcsLens);

}

const int MAX_LINE_NUM_LIMIT = 10000;

//与cmp一样，不过每次比较的是一个QByteArray，而不是一个char
//返回的是最大行LCS的md4编码值。根据该编码是否相等，来决定文件是否相等。
void CmpareMode::cmpByLine(AbstractCompare *blockCompare)
{
	if (m_leftLineInfoVec.size() >= MAX_LINE_NUM_LIMIT && m_rightLineInfoVec.size() >= MAX_LINE_NUM_LIMIT)
	{
		QVector<QByteArray>pLineLeft;
		QVector<QByteArray>pLineRight;

		int count = 0;
		int leftLineNum = 0;
		int rightLineNum = 0;

		int times = 10;
		int max_line_length_limit = 200;

		bool isNeedRetry = true;

		QMap<int, EqualLineInfo> enqualLineInfoVec;
		QList<EqualLineInfo> tempList;

		while (isNeedRetry && times > 0)
		{
			while (times > 0)
			{
				leftLineNum = 0;
				rightLineNum = 0;

				pLineLeft.clear();

				count = m_leftLineInfoVec.size();

				for (int i = 0; i < count; ++i)
				{
					if (m_leftLineInfoVec.at(i).unicodeStr.size() >= max_line_length_limit)
					{
						pLineLeft.append(m_leftLineInfoVec[i].md4);
						++leftLineNum;
					}
				}


				count = m_rightLineInfoVec.size();

				pLineRight.clear();

				for (int i = 0; i < count; ++i)
				{
					if (m_rightLineInfoVec.at(i).unicodeStr.size() >= max_line_length_limit)
					{
						pLineRight.append(m_rightLineInfoVec[i].md4);
						++rightLineNum;
					}
				}

				--times;

				//这里是有可能失败的，如果都小于200
				if ((leftLineNum == 0 || rightLineNum == 0) && max_line_length_limit > 20)
				{
					//重新来一次
					max_line_length_limit -= 20;
				}
				else
				{
					break;
				}
			}

			//找出长度大于200的lcs字段
			//每一行的md4值组成一个数组，做一个LCS计算
			LcsLine lcs(pLineLeft.data(), pLineRight.data(), leftLineNum, rightLineNum);
			lcs.setCancelFlag(blockCompare->m_isCancel);
			connect(&lcs, &LcsLine::outMsg, this, &CmpareMode::outputMsg);

		/*	if (m_lcsLine != nullptr)
			{
				delete[] m_lcsLine;
				m_lcsLine = nullptr;
			}*/
			int lcsLens = 0;

			QByteArray* lcsLine = lcs.cmp(lcsLens);

			pLineLeft.clear();
			pLineRight.clear();


			if (lcsLens == 0)
			{
				goto normal_lcs;
			}

			enqualLineInfoVec.clear();

			cmpDateAndLcsLine(m_leftLineInfoVec, 0, lcsLine, lcsLens, RC_LEFT, enqualLineInfoVec);
			cmpDateAndLcsLine(m_rightLineInfoVec, 0, lcsLine, lcsLens, RC_RIGHT, enqualLineInfoVec);

			if (lcsLine != nullptr)
			{
				delete[]lcsLine;
			}

			//把enqualLineInfoVec中内容分块，这样就把一个大的lcs，划分为多个块来计算，降低整体耗时。
			tempList = enqualLineInfoVec.values();

			int lastLine = 0;
			isNeedRetry = false;

			for (int i = 0; i < tempList.size(); ++i)
			{
				if (tempList.at(i).leftLineNums - lastLine >= MAX_LINE_NUM_LIMIT)
				{
					//还需要进一步缩小，否则还是慢。前后差距超过万行
					isNeedRetry = true;
					break;
				}
				lastLine = tempList.at(i).leftLineNums;
			}

			if (isNeedRetry && (max_line_length_limit > 20))
			{
				tempList.clear();

				for (int i = 0; i < m_leftLineInfoVec.size(); ++i)
				{
					m_leftLineInfoVec[i].isLcsExist = false;
				}
				for (int i = 0; i < m_rightLineInfoVec.size(); ++i)
				{
					m_rightLineInfoVec[i].isLcsExist = false;
				}
				max_line_length_limit -= 20;
			}
			else
			{
				break;
			}
		}

		int leftBlockStartIndex = 0;
		int rightBlockStartIndex = 0;

		auto findIndex = [](QList<LineFileInfo>& lineInfo, int lineNums) {
			for (int i = 0; i < lineInfo.size(); ++i)
			{
				if (lineInfo.at(i).lineNums == lineNums)
				{
					return i;
				}
			}
			return -1;
		};

		//QVector< QByteArray> lcsData;

		m_enqualLineInfoVec.clear();

		int leftBlockEndIndex = 0;
		int rightBlockEndIndex = 0;

		for (int i = 0; i <= tempList.size(); ++i)
		{
			if (i < tempList.size())
			{
				int leftBlockEndLineNum = tempList.at(i).leftLineNums;
				int rightBlockEndLineNum = tempList.at(i).rightLineNums;

				leftBlockEndIndex = findIndex(m_leftLineInfoVec, leftBlockEndLineNum);
				rightBlockEndIndex = findIndex(m_rightLineInfoVec, rightBlockEndLineNum);
			}
			else if(i == tempList.size())
			{
				//如果尾部还有内容，则要进行最后一次lcs求算
				if (leftBlockStartIndex < m_leftLineInfoVec.size() && rightBlockEndIndex < m_rightLineInfoVec.size())
				{
					leftBlockEndIndex = m_leftLineInfoVec.size() - 1;
					rightBlockEndIndex = m_rightLineInfoVec.size() - 1;
				}
				else
				{
					break;
				}
			}

			//从leftBlockStartIndex到leftBlockEndIndex，做一个块，进行lcs的处理。（包含leftBlockEndIndex)
			pLineLeft.clear();

			for (int index = leftBlockStartIndex; index <= leftBlockEndIndex; ++index)
			{
				pLineLeft.append(m_leftLineInfoVec[index].md4);
			}

			pLineRight.clear();
			for (int index = rightBlockStartIndex; index <= rightBlockEndIndex; ++index)
			{
				pLineRight.append(m_rightLineInfoVec[index].md4);
			}

			LcsLine lcs(pLineLeft.data(), pLineRight.data(), pLineLeft.size(), pLineRight.size());
			lcs.setCancelFlag(blockCompare->m_isCancel);
			connect(&lcs, &LcsLine::outMsg, this, &CmpareMode::outputMsg);


			int lcsLens = 0;
			QByteArray* lcsLine = lcs.cmp(lcsLens);

			enqualLineInfoVec.clear();

			cmpDateAndLcsLine(m_leftLineInfoVec, leftBlockStartIndex, lcsLine, lcsLens, RC_LEFT, enqualLineInfoVec);
			cmpDateAndLcsLine(m_rightLineInfoVec, rightBlockStartIndex, lcsLine, lcsLens, RC_RIGHT, enqualLineInfoVec);

			if (lcsLine != nullptr)
			{
				delete[]lcsLine;
			}
			int offset = m_enqualLineInfoVec.size();

			for (int k = 0; k < lcsLens; ++k)
			{
				m_enqualLineInfoVec.insert(offset + k, enqualLineInfoVec.value(k));
			}

			leftBlockStartIndex = leftBlockEndIndex + 1;
			rightBlockStartIndex = rightBlockEndIndex + 1;
		}
		return;
	}
normal_lcs:
	{
	QByteArray *pLineLeft = new QByteArray[m_leftLineInfoVec.size()];

	int count = m_leftLineInfoVec.size();

	for (int i = 0; i < count; ++i)
	{
		pLineLeft[i] = m_leftLineInfoVec[i].md4;
	}


	QByteArray *pLineRight = new QByteArray[m_rightLineInfoVec.size()];

	count = m_rightLineInfoVec.size();

	for (int i = 0; i < count; ++i)
	{
		pLineRight[i] = m_rightLineInfoVec[i].md4;
	}

	//每一行的md4值组成一个数组，做一个LCS计算
	LcsLine lcs(pLineLeft, pLineRight, m_leftLineInfoVec.size(), m_rightLineInfoVec.size());
	lcs.setCancelFlag(blockCompare->m_isCancel);
	connect(&lcs, &LcsLine::outMsg, this, &CmpareMode::outputMsg);

	if (m_lcsLine != nullptr)
	{
		delete [] m_lcsLine;
		m_lcsLine = nullptr;
	}
	int retLength = 0;

	m_lcsLine = lcs.cmp(retLength);

	delete[] pLineLeft;
	delete[] pLineRight;

	cmpLcsLine(m_lcsLine, retLength);
}

}

/*2020-4-11发现一个问题，如果是\r\n分开两个，会换行2次，所以需要将\r\n一次性输出
* 该函数只用于第一次输出时。如果后续编辑的块再输出，使用outputEditBlock。这两种情况虽然很类似，但是还是有差异。
*主要差异在于：第一次新输出时，文档是空的，每次修改的第一行都是一个崭新的空白行；而编辑后类似于插入，插入的地方不是
*一个新行，而是已经存在一个行。这两种情况下，有些格式设置，比如背景色，对齐色等会不同。
*/
void CmpareMode::outputBlock(int direction, QVector<BlockNode> &blocks, bool isEqualBlock)
{
	QStringList *content = (direction == LEFT) ? m_leftContent : m_rightContent;

	QVector<UnequalCharsPosInfo>& m_unqealPosList = (direction == LEFT) ? m_leftUnqealPosList : m_rightUnqealPosList;

	int blockType = UNKNOWN_BLOCK;

	int &curCharPos = (direction == LEFT) ? m_leftCurPos : m_rightCurPos;

	QList<BlockUserData*>& externBlockInfo = (direction == LEFT) ? m_leftExternBlockInfo : m_rightExternBlockInfo;

	for (int i = 0; i < blocks.size(); ++i)
	{
		const BlockNode & t = blocks.at(i);

		if (t.type == BlockType::REAL_DATA)
		{
			//实际数据块
			for (int j = 0; j < t.lineNodes.count(); ++j)
			{
				const LineNode & lines = t.lineNodes.at(j);


				//虽然是不相等块，但是行却完全相同，还是按照相等块对待
				//这种情况大概率出现在左右两边都是空白行的情况
				//相等块或者左右行完全相同，均作为相等块输出
				if (isEqualBlock || lines.totalEqual)
				{
					blockType = EQUAL_BLOCK;
				}
				//不相等的块需要设置红色背景块
				else
				{
					blockType = UNEQUAL_BLOCK;
				}

				///* 每个块都设置User Data,不再使用userStatus */
				BlockUserData *userData = new BlockUserData(blockType);
				userData->setSrcBlockNum(content->size());
				userData->m_lineEndType = (int)getLineEndType(lines);

				externBlockInfo.append(userData);


				QString oneLineContens;

				for (int k = 0; k < lines.lineText.count(); ++k)
				{
					const SectionNode & section = lines.lineText.at(k);

					/* 2020-6-14 发现：\r\n行尾在textCursor里面只算一个QChar字符8233 ；而且每次插入一行（带换行符）后，块就会自动加1块
					* 这里有个问题，上面如果设置setNoEqualBackColor颜色后，再下面调用insertPlainText插入时，如果插入的带有换行，肯定会新
					* 加一块，这个块的颜色也会被之前设置setNoEqualBackColor后颜色所覆盖。但是由于下次调用会重新设置，所以之前没有暴露问题。
					*/
					if (section.equal)
					{

						oneLineContens.append(section.text);
					}
					else
					{
						oneLineContens.append(section.text);

						UnequalCharsPosInfo pos;
						pos.start = curCharPos;
						pos.length = section.text.toUtf8().size();

						m_unqealPosList.append(pos);
					}

					/*2021/6/12发现必须要把qstring转换为uft8,然后统计字节长度才行；如果直接用qstring.size则会在中文下对比错误；
					*我们是把不管什么字符编码原始文件，都转换为unicode这样一个统一的编码后，再去对比的。
					*/
					curCharPos += section.text.toUtf8().size();
				}

				content->append(oneLineContens);
			}
		}
		else if (t.type == BlockType::PAD_DATA)
		{
			for (int k = 0; k < t.alignRowCount; ++k)
			{
				BlockUserData *userData = new BlockUserData(PAD_BLOCK);
				userData->setSrcBlockNum(content->size());
				userData->m_lineEndType = UNKNOWN_LINE;//其实应该是PAD_LINE，为了和未知行位保持一致，只能写UNKNOWN_LINE
				externBlockInfo.append(userData);

				//还要加一个苹果的才好
#ifdef _WIN32
				content->append(QString("\r\n"));
#else
				content->append(QString("\n"));
#endif
			}
#ifdef _WIN32
			curCharPos += 2*t.alignRowCount;
#else
			curCharPos += t.alignRowCount;
#endif
		}

	}
}


void CmpareMode::getResult(QStringList* & leftContent, QStringList * &rightContent)
{
	leftContent = m_leftContent;
	rightContent = m_rightContent;
}

void CmpareMode::getUnqealPosList(QVector<UnequalCharsPosInfo>*& left, QVector<UnequalCharsPosInfo>*& right)
{
	left = &m_leftUnqealPosList;
	right = &m_rightUnqealPosList;
}


void CmpareMode::outputBlocks(AbstractCompare *blockCompare)
{

	if (blockCompare->m_isCancel != nullptr && *(blockCompare->m_isCancel))
	{
		return;
	}

	int i = 0;

	bool b1 = true;
	bool b2 = true;

	QVector<BlockNode> leftBlocks;
	QVector<BlockNode> rightBlocks;

	QList<LineFileInfo> leftLineIndex;
	QList<LineFileInfo> rightLineIndex;

	getLines(RC_LEFT, leftLineIndex);
	getLines(RC_RIGHT, rightLineIndex);

	m_leftContent->clear();
	m_rightContent->clear();

	while (b1 || b2)
	{
		//交叉输出不等块 与 相等块

		//不等块
		if (i < m_leftNoEqualBlockInfoVec.size())
		{

			leftBlocks.clear();
			rightBlocks.clear();

			blockCompare->blockCmpLcs(m_leftNoEqualBlockInfoVec.at(i), leftLineIndex, \
				m_rightNoEqualBlockInfoVec.at(i), rightLineIndex, leftBlocks, rightBlocks);
			outputBlock(LEFT, leftBlocks, false);
			outputBlock(RIGHT, rightBlocks, false);
		}
		else
		{
			b2 = false;
		}

		//相等块
		if (i < m_leftEqualBlockInfoVec.size())
		{

			leftBlocks.clear();
			rightBlocks.clear();

			blockCompare->createBlockWithMultiEqualLines(m_leftEqualBlockInfoVec.at(i), leftLineIndex, \
				m_rightEqualBlockInfoVec.at(i), rightLineIndex, \
				leftBlocks, rightBlocks);

			outputBlock(LEFT, leftBlocks, true);
			outputBlock(RIGHT, rightBlocks, true);

		}
		else
		{
			b1 = false;
		}


		++i;
	}
}



void CmpareMode::getLinesExternInfo(QList<BlockUserData*>*& leftExternBlockInfo, QList<BlockUserData*>*& rightExternBlockInfo)
{
	leftExternBlockInfo = &m_leftExternBlockInfo;
	rightExternBlockInfo = &m_rightExternBlockInfo;
}


//目前只用在目录对比的深度继续对比中。打开文件，跳过空行，累加文件后，进行md5sum处理。
//几乎就是从preScanFile中逻辑拷贝而来。
bool CmpareMode::cmpSkipEmptyLine()
{
	bool ret = false;

	//默认忽略行前行后的所有空白
	m_mode = 2;

	//如果文件是python类，//默认只忽略行尾的空白。避免python这类对行前字符敏感的文件。
	if (m_leftFilePath.endsWith(".py") || m_rightFilePath.endsWith(".py"))
	{
		m_mode = 1;
	}

	if (m_leftFile != nullptr && m_rightFile != nullptr && m_leftFpr != 0 && m_rightFpr != 0)
	{
		ret = true;

		CODE_ID code = getTextFileEncodeType(RC_LEFT);
		int maxLineSize = 0;

		if (code == UNICODE_LE)
		{
			m_leftCode = UNICODE_LE;
			readLineFromFileWithUnicodeLe(m_leftFpr, m_leftContentLength, m_leftLineInfoVec, m_leftBlankLineInfoVec, m_mode, maxLineSize);
		}
		else
		{
			m_leftCode = readLineFromFile(m_leftFpr, m_leftContentLength, code, m_leftLineInfoVec, m_leftBlankLineInfoVec, m_mode, maxLineSize);
		}

		//UNICODE_LE格式需要单独处理
		code = getTextFileEncodeType(RC_RIGHT);

		if (code == UNICODE_LE)
		{
			m_rightCode = UNICODE_LE;
			readLineFromFileWithUnicodeLe(m_rightFpr, m_rightContentLength, m_rightLineInfoVec, m_rightBlankLineInfoVec, m_mode, maxLineSize);
		}
		else
		{
			m_rightCode = readLineFromFile(m_rightFpr, m_rightContentLength, code, m_rightLineInfoVec, m_rightBlankLineInfoVec, m_mode, maxLineSize);
		}
		//提出空白后行后，如果都相等，则认定相等。
		if (m_leftLineInfoVec.size() == m_rightLineInfoVec.size())
		{
			//大小相等，则判断每一行的md5
			for (int i = 0, s = m_leftLineInfoVec.size(); i < s; ++i)
			{
				if (m_leftLineInfoVec.at(i).md4 != m_rightLineInfoVec.at(i).md4)
				{
					ret = false;
					break;
				}
			}
		}
		else
		{
			ret = false;
		}


	}
	return ret;
}
