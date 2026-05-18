#pragma once
#include "command.h"
#include "rcglobal.h"
#include "blockuserdata.h"

class CompareWin;


//记录每一步的操作
class OperatorRecord {

public:
	RC_DIRECTION dir; //RC_LEFT 左边发起，去同步右边 反之

	OperRecordStatus operatorType; //操作类型 1 同步
	OperatorInfo leftOperator;
	OperatorInfo rightOperator;

	//关于操作的描述
	QString operatorDesc;

	~OperatorRecord()
	{
		for (int j = 0; j < leftOperator.lineContents.size(); ++j)
		{
			delete leftOperator.lineContents.at(j);
		}

		for (int k = 0; k < leftOperator.lineExternInfo.size(); ++k)
		{
			delete leftOperator.lineExternInfo.at(k);
		}

		for (int j = 0; j < rightOperator.lineContents.size(); ++j)
		{
			delete rightOperator.lineContents.at(j);
		}

		for (int k = 0; k < rightOperator.lineExternInfo.size(); ++k)
		{
			delete rightOperator.lineExternInfo.at(k);
		}
	}
	OperatorRecord() = default;
	OperatorRecord(const OperatorRecord& other) = delete;

};

class AsyncCommand :public Command
{
public:
	AsyncCommand(CompareWin* operWin);
	virtual ~AsyncCommand();

	virtual int getOperIndex()override;
	virtual void undo()override;


	virtual QString desc()override;
	void setDesc(QString v);

	void setOperIndex(int v);
	void addRecord(OperatorRecord* v);

private:
	CompareWin* m_operWin;
	QList<OperatorRecord*> m_record;
	int m_index;
	QString m_desc;

	AsyncCommand(const AsyncCommand& o) = delete;
	AsyncCommand& operator=(const AsyncCommand& o) = delete;
};

