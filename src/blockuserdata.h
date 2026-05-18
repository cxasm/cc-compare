#pragma once
#include <qtextobject.h>
#include <QPushButton>

class BlockUserData
{
public:

	explicit BlockUserData(int blockType);
	BlockUserData(int noEqualFlag,int blockLen);
	BlockUserData(int blockType, int lineEndType, int blockLen);
	BlockUserData(const BlockUserData & other);

	virtual ~BlockUserData();

	bool operator==(const BlockUserData & other) const ;
	bool operator!=(const BlockUserData & other) const ;

	void setParam(int noEqualFlag,int blockLen = 1);

	int m_MarkerFlag;//0表示没有 1 表示箭头 2 表示垂直线

	int m_blockType; //块的类型/相等/不等/空白见 BLOCKSTATUS 枚举状态

	int m_blockLen;

	//都是从0开始的，不是从1开始
	int m_srcBlockNum;

	int m_lineEndType; //行尾换行类型

	void setSrcBlockNum(int blockNum)
	{
		m_srcBlockNum = blockNum;
	}
private:
	BlockUserData& operator=(const BlockUserData& other) = delete;
};

