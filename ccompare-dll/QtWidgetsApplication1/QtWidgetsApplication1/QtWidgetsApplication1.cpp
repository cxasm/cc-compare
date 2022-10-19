#include "QtWidgetsApplication1.h"
#include <ccmp.h>


QtWidgetsApplication1::QtWidgetsApplication1(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

	CCmp* pcmp = new CCmp(this);
	
#if 0
	//文件同步的调用方式，会弹出对比界面，可进行同步
	pcmp->compareSyncFile(QString(".//testfile/1.txt"), QString(".//testfile/2.txt"));
#else
	//文件对比的方式，自己获取不同点后，进行自定义渲染显示。比较完成后，会发出cmpResult信号
	connect(pcmp, &CCmp::cmpFinished, this, &QtWidgetsApplication1::on_cmpSuccess);
	pcmp->compareFile(QString(".//testfile/1.txt"), QString(".//testfile/2.txt"));
#endif
}

#if 0
//在这里进行自定义绘制。1个简单的例子，输出文笔，使用红色笔标出不同部分
void QtWidgetsApplication1::on_cmpSuccess(int resultType, QStringList* leftContents, QStringList* rightContents, QVector<UnequalCharsPosInfo>* leftUnequalInfo, QVector<UnequalCharsPosInfo>* rightUnequalInfo, \
	const QList<BlockUserData*>* leftBlockData, const QList<BlockUserData*>* rightBlockData)
{
	if (resultType == 0)
	{
		//对比正常返回，自定义处理结果
		//先把左右两边的内容,显示在左右两个textbrower中。

		//左边的每一行都在leftContents中，右边在rightContents中
		QString leftText = leftContents->join("");
		QString rightText = rightContents->join("");

		QByteArray leftUtf8Chars = leftText.toUtf8();
		
		//下面依次把text写入leftTextEdit，但是不同的部分，使用红色颜色来写入
		//注意文字要按照utf8的格式来进行写入，leftUnequalInfo中所有的长度都是按照utf8来进行统一规划的。

		const QColor equalColor = QColor("black");
		const QColor unequalColor = QColor("red");

		int curPos = 0;
		for (int i = 0, s = leftUnequalInfo->size(); i < s; ++i)
		{
			const UnequalCharsPosInfo& unequalinfo = leftUnequalInfo->at(i);

			//先写入相同的部分
			if (curPos < unequalinfo.start)
			{
				ui.leftTextEdit->setTextColor(equalColor);
				QString text = QString(leftUtf8Chars.mid(curPos, unequalinfo.start - curPos));
				ui.leftTextEdit->insertPlainText(text);
			}

			if (unequalinfo.length > 0)
			{
				//切换为红笔，在写入不同部分
				ui.leftTextEdit->setTextColor(unequalColor);
				QString text = QString(leftUtf8Chars.mid(unequalinfo.start, unequalinfo.length));
				ui.leftTextEdit->insertPlainText(text);
			}

			curPos = unequalinfo.start + unequalinfo.length;
		}

		//再写入右边的不同
		curPos = 0;
		QByteArray rightUtf8Chars = rightText.toUtf8();
		for (int i = 0, s = rightUnequalInfo->size(); i < s; ++i)
		{
			const UnequalCharsPosInfo& unequalinfo = rightUnequalInfo->at(i);

			//先写入相同的部分
			if (curPos < unequalinfo.start)
			{
				ui.rightTextEdit->setTextColor(equalColor);
				QString text = QString(rightUtf8Chars.mid(curPos, unequalinfo.start - curPos));
				ui.rightTextEdit->insertPlainText(text);
			}

			if (unequalinfo.length > 0)
			{
				//切换为红笔，在写入不同部分
				ui.rightTextEdit->setTextColor(unequalColor);
				QString text = QString(rightUtf8Chars.mid(unequalinfo.start, unequalinfo.length));
				ui.rightTextEdit->insertPlainText(text);
			}

			curPos = unequalinfo.start + unequalinfo.length;
		}

		//所有入参参数不需要回收。框架会自动回收

	}
	else if (resultType == 1)
	{
		//其中一个文件是空内容的文件，不需要进行对比。
		//直接把原始文件读取后显示出来即可。忽略实现
	}
}
#else


enum BLOCKSTATUS {
	UNKNOWN_BLOCK = 0, //未知
	EQUAL_BLOCK = 1,//相等
	UNEQUAL_BLOCK, //不等
	PAD_BLOCK, //对齐
	LAST_PAD_EMPTY_BLOCK, // 最后一个用于对齐的空行，只在最后一行可能出现
	TEMP_INSERT_BLOCK //临时插入块
};


//在这里进行自定义绘制。一个复杂的例子，在行的前面标注行的性质：行号：对齐行[pad]、不等行[x]。（相等等不标注）
void QtWidgetsApplication1::on_cmpSuccess(int resultType, QStringList* leftContents, QStringList* rightContents, QVector<UnequalCharsPosInfo>* leftUnequalInfo, QVector<UnequalCharsPosInfo>* rightUnequalInfo, \
	const QList<BlockUserData*>* leftBlockData, const QList<BlockUserData*>* rightBlockData)
{
	if (resultType == 0)
	{
		//对比正常返回，自定义处理结果
		//先把左右两边的内容,显示在左右两个textbrower中。

		//下面依次把text写入leftTextEdit，但是不同的部分，使用红色颜色来写入
		//注意文字要按照utf8的格式来进行写入，leftUnequalInfo中所有的长度都是按照utf8来进行统一规划的。

		const QColor equalColor = QColor("black");
		const QColor unequalColor = QColor("red");

		int curPos = 0;

		//求目前该行中的不同点的区间
		auto getUnEqualBlock = [](int pos, int lineLength, QVector<UnequalCharsPosInfo>* unequalInfo)->QList< UnequalCharsPosInfo > {

			int oldPos = pos;

			QList< UnequalCharsPosInfo >ret;

			for (int i = 0; i < unequalInfo->size() && lineLength >0; ++i)
			{
				const UnequalCharsPosInfo & unequal = unequalInfo->at(i);

				if (pos + lineLength <= unequal.start)
				{
					break;
				}
				if (pos >= unequal.start + unequal.length)
				{
					continue;
				}

				//如果有公共段，只可能是UnequalCharsPosInfo落在线段中间的那种。不可能是交叉相交。
				int a1 = pos;
				int a2 = pos + lineLength;
				int b1 = unequal.start;
				int b2 = unequal.start + unequal.length;

				if (b2 >= a1 && a2 >= b1)
				{
					UnequalCharsPosInfo r;
					r.start = qMax(a1, b1);
					r.length = qMin(a2, b2) - r.start;
					if (r.length > 0)
					{
					r.start = r.start - oldPos;
					ret.append(r);
				}
			}
			}
			return ret;
		};

		//一行一行的来写入左边文本
		for (int i = 0; i < leftContents->size(); ++i)
		{
			//先查询该行是什么行
			int type = leftBlockData->at(i)->m_blockType;

			switch (type)
			{
			case EQUAL_BLOCK://相等行，直接输入文本
			{
				ui.leftTextEdit->setTextColor(equalColor);
				ui.leftTextEdit->insertPlainText(QString("%1 ").arg(i) + leftContents->at(i));

				curPos += leftContents->at(i).toUtf8().length();
			}
				break;
			case UNEQUAL_BLOCK://不等行，前面加入[x]
			{
				ui.leftTextEdit->setTextColor(unequalColor);
				ui.leftTextEdit->insertPlainText(QString("%1 [x]").arg(i));

				

				QByteArray byte = leftContents->at(i).toUtf8();
				int lineLens = byte.length();

				QList< UnequalCharsPosInfo > unequalInfo = getUnEqualBlock(curPos, byte.length(), leftUnequalInfo);

				if (unequalInfo.isEmpty())
				{
					ui.leftTextEdit->setTextColor(equalColor);
					ui.leftTextEdit->insertPlainText(leftContents->at(i));
				}
				else
				{
					int linePos = 0;
					
					for (int i = 0; i < unequalInfo.size(); ++i)
					{
						const UnequalCharsPosInfo& unequalinfo = unequalInfo.at(i);

						//先写入相同的部分
						if (linePos < unequalinfo.start)
						{
							ui.leftTextEdit->setTextColor(equalColor);
							QString text = QString(byte.mid(linePos, unequalinfo.start - linePos));
							ui.leftTextEdit->insertPlainText(text);
						}

						if (unequalinfo.length > 0)
						{
							//切换为红笔，在写入不同部分
							ui.leftTextEdit->setTextColor(unequalColor);
							QString text = QString(byte.mid(unequalinfo.start, unequalinfo.length));
							ui.leftTextEdit->insertPlainText(text);
						}

						linePos = unequalinfo.start + unequalinfo.length;
					}
					if (linePos < lineLens)
					{
						ui.leftTextEdit->setTextColor(equalColor);
						QString text = QString(byte.mid(linePos,-1));
						ui.leftTextEdit->insertPlainText(text);
					}
				}

				curPos += lineLens;
			}
				break;
			case PAD_BLOCK://对齐行，前面加入[pad]
				ui.leftTextEdit->setTextColor(unequalColor);
				ui.leftTextEdit->insertPlainText(QString("%1 [pad]").arg(i) + leftContents->at(i));
				curPos += leftContents->at(i).toUtf8().length();
				break;
			case LAST_PAD_EMPTY_BLOCK://可能存在的最后一个补齐行，前面加入[lastpad]
				ui.leftTextEdit->insertPlainText(QString("%1 [lastpad]").arg(i) + leftContents->at(i));
				curPos += leftContents->at(i).toUtf8().length();
				break;
			default:
				break;
			}
		}
		
		curPos = 0;
		//一行一行的来写入右边文本
		for (int i = 0; i < rightContents->size(); ++i)
		{
			//先查询该行是什么行
			int type = rightBlockData->at(i)->m_blockType;

			switch (type)
			{
			case EQUAL_BLOCK://相等行，直接输入文本
			{
				ui.rightTextEdit->setTextColor(equalColor);
				ui.rightTextEdit->insertPlainText(QString("%1 ").arg(i) + rightContents->at(i));

				curPos += rightContents->at(i).toUtf8().length();
			}
			break;
			case UNEQUAL_BLOCK://不等行，前面加入[x]
			{
				ui.rightTextEdit->setTextColor(unequalColor);
				ui.rightTextEdit->insertPlainText(QString("%1 [x]").arg(i));

				QByteArray byte = rightContents->at(i).toUtf8();
				QList< UnequalCharsPosInfo > unequalInfo = getUnEqualBlock(curPos, byte.length(), rightUnequalInfo);
				int lineLens = byte.length();;

				if (unequalInfo.isEmpty())
				{
					ui.rightTextEdit->setTextColor(equalColor);
					ui.rightTextEdit->insertPlainText(rightContents->at(i));
				}
				else
				{
					int linePos = 0;
					for (int i = 0; i < unequalInfo.size(); ++i)
					{
						const UnequalCharsPosInfo& unequalinfo = unequalInfo.at(i);

						//先写入相同的部分
						if (linePos < unequalinfo.start)
						{
							ui.rightTextEdit->setTextColor(equalColor);
							QString text = QString(byte.mid(linePos, unequalinfo.start - linePos));
							ui.rightTextEdit->insertPlainText(text);
						}

						if (unequalinfo.length > 0)
						{
							//切换为红笔，在写入不同部分
							ui.rightTextEdit->setTextColor(unequalColor);
							QString text = QString(byte.mid(unequalinfo.start, unequalinfo.length));
							ui.rightTextEdit->insertPlainText(text);
						}

						linePos = unequalinfo.start + unequalinfo.length;
					}
					if (linePos < lineLens)
					{
						ui.rightTextEdit->setTextColor(equalColor);
						QString text = QString(byte.mid(linePos, -1));
						ui.rightTextEdit->insertPlainText(text);
					}
				}
				
				curPos += lineLens;
			}
			break;
			case PAD_BLOCK://对齐行，前面加入[pad]
				ui.rightTextEdit->setTextColor(unequalColor);
				ui.rightTextEdit->insertPlainText(QString("%1 [pad]").arg(i) + rightContents->at(i));
				curPos += rightContents->at(i).toUtf8().length();
				break;
			case LAST_PAD_EMPTY_BLOCK://可能存在的最后一个补齐行，前面加入[lastpad]
				ui.rightTextEdit->insertPlainText(QString("%1 [lastpad]").arg(i) + rightContents->at(i));
				curPos += rightContents->at(i).toUtf8().length();
				break;
			default:
				break;
			}
		}

	}
	else if (resultType == 1)
	{
		//其中一个文件是空内容的文件，不需要进行对比。
		//直接把原始文件读取后显示出来即可。忽略实现
	}
}
#endif
