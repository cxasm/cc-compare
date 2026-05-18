#pragma once
#include "command.h"
#include "rcglobal.h"


//记录undo与redo的操作记录的


class ModifyOperRecords {
public:
	RC_DIRECTION dir;
	int position;//当前修改位置
	int modificationType;//1：增加 2 删除
	int length;//修改的长度
	int linesAdded;//增加多少行。正为增加，负数为减少
	char* contents;
	ModifyOperRecords(int position_, int type_, int length_, int linesAdded_) :position(position_), modificationType(type_), length(length_), linesAdded(linesAdded_), contents(nullptr)
	{

	}

	~ModifyOperRecords()
	{
		if (contents != nullptr)
		{
			delete[]contents;
			contents = nullptr;
		}
	}

	ModifyOperRecords() = default;
	ModifyOperRecords(const ModifyOperRecords& other) = delete;

};


class CompareWin;
class EditCommand :public Command
{
public:
	EditCommand(CompareWin* operWin);
	virtual ~EditCommand();

	virtual int getOperIndex()override;
	virtual void undo()override;

	virtual QString desc()override;
	void setDesc(QString v);

	void addRecord(ModifyOperRecords* v);
	void setOperIndex(int v);


private:
	QList<ModifyOperRecords*> m_record;
	CompareWin* m_operWin;
	int m_index;

	QString m_desc;

	EditCommand(const EditCommand& o) = delete;
	EditCommand& operator=(const EditCommand& o) = delete;
};

