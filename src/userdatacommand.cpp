#include "userdatacommand.h"
#include "command.h"
#include "comparewin.h"


//用来取消行用户信息的设置
UserDataCommand::UserDataCommand(CompareWin* operWin) :m_operWin(operWin), m_index(0)
{
}

UserDataCommand::~UserDataCommand()
{
    for (auto var : m_record)
    {
        delete var;
    }
}

int UserDataCommand::getOperIndex()
{
    return m_index;
}

void UserDataCommand::undo()
{
	//最后一个元素，设置为真。userdata本身就是在最后做的一个恢复操作
	if (!m_record.isEmpty())
	{
		m_record.last()->isLastOne = true;
	}

    for (UserDataOperRecords* var : m_record)
    {
        m_operWin->undoUserData(var);

        delete var;
    }

    m_record.clear();

    m_operWin->autoAdjustLineUserData(RC_LEFT);
    m_operWin->autoAdjustLineUserData(RC_RIGHT);

    m_operWin->autoAdjustMargins();
    
}

QString UserDataCommand::desc()
{
    return m_desc;
}

void UserDataCommand::setDesc(QString v)
{
    m_desc = v;
}

void UserDataCommand::setOperIndex(int v)
{
    m_index = v;
}

void UserDataCommand::addRecord(UserDataOperRecords* v)
{
    m_record.push_front(v);
}
