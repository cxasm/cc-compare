#pragma once
#include <qtextobject.h>
#include <QPushButton>

/*
enum BLOCKSTATUS {
	UNKNOWN_BLOCK = 0, //未知
	EQUAL_BLOCK = 1,//相等
	UNEQUAL_BLOCK, //不等
	PAD_BLOCK, //对齐
	LAST_PAD_EMPTY_BLOCK, // 最后一个用于对齐的空行，只在最后一行可能出现
	TEMP_INSERT_BLOCK //临时插入块
};
*/

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

