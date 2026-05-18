#include "blockuserdata.h"
#include "rcglobal.h"


BlockUserData::BlockUserData(int noEqualFlag, int blockLen):m_MarkerFlag(noEqualFlag),m_blockLen(blockLen), m_lineEndType(UNKNOWN_LINE), m_blockType(UNKNOWN_BLOCK)
{
	m_srcBlockNum = -1;
}

BlockUserData::BlockUserData(int blockType): m_blockType(blockType), m_MarkerFlag(MARGIN_NONE), m_lineEndType(UNKNOWN_LINE), m_blockLen(0)
{
	m_srcBlockNum = -1;
}

/* 最后两个参数完全是为了避免重载冲突 */
BlockUserData::BlockUserData(int blockType, int lineEndType, int /*blockLen*/):m_MarkerFlag(MARGIN_NONE),m_blockType(UNKNOWN_BLOCK)
{
	m_blockType = blockType;
	m_lineEndType = lineEndType;
}

BlockUserData::BlockUserData(const BlockUserData & other)
{
	m_blockType = other.m_blockType;
	m_blockLen = other.m_blockLen;
	m_srcBlockNum = other.m_srcBlockNum;
	m_lineEndType = other.m_lineEndType;
	m_MarkerFlag = other.m_MarkerFlag;
}


BlockUserData::~BlockUserData()
{
	//qDebug("del userdata  %p src num %d", this, m_srcBlockNum);
}


bool BlockUserData::operator==(const BlockUserData &other) const
{
	return ((m_blockType == other.m_blockType)&& (m_MarkerFlag == other.m_MarkerFlag) && (m_blockLen == other.m_blockLen));
}


bool BlockUserData::operator!=(const BlockUserData &other) const
{
	return !(*this == other);
}


void BlockUserData::setParam(int markerFlag, int blockLen)
{
	m_MarkerFlag = markerFlag;
	m_blockLen = blockLen;
}
