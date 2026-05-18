#pragma once
#include "command.h"
#include "rcglobal.h"
#include "blockuserdata.h"
#include <QList>


class CompareWin;


enum USER_DATA_OPER_TYPE {
	DEL_USER_DATA = 1,
	ADD_USER_DATA
};

//记录undo与redo的操作记录的userdata相关操作
class UserDataOperRecords {
public:
	RC_DIRECTION dir;

	int operType; //1DEL_USER_DATA 2ADD_USER_DATA

	QList<BlockUserData*> userData;

	UserDataOperRecords(RC_DIRECTION dir_) :dir(dir_), isLastOne(false)
	{

	}

	~UserDataOperRecords()
	{
		for (int j = 0; j < userData.size(); ++j)
		{
			delete userData.at(j);
		}
		userData.clear();
	}

	UserDataOperRecords() = default;
	UserDataOperRecords(const UserDataOperRecords& other) = delete;

	//记录是否是最后一个操作，因为最后一个要做一个额外的操作
	bool isLastOne;

};

class UserDataCommand :public Command
{

public:
	UserDataCommand(CompareWin* operWin);
	virtual ~UserDataCommand();

	virtual int getOperIndex()override;
	virtual void undo()override;


	virtual QString desc()override;
	void setDesc(QString v);

	void setOperIndex(int v);
	void addRecord(UserDataOperRecords* v);

private:
	CompareWin* m_operWin;
	QList<UserDataOperRecords*> m_record;
	int m_index;
	QString m_desc;

	UserDataCommand(const UserDataCommand& o) = delete;
	UserDataCommand& operator=(const UserDataCommand& o) = delete;

};

