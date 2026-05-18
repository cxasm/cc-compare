#include "asynccommand.h"
#include "command.h"
#include "comparewin.h"

AsyncCommand::AsyncCommand(CompareWin* operWin):m_operWin(operWin), m_index(0)
{
}

AsyncCommand::~AsyncCommand()
{
    for (auto var : m_record)
    {
        delete var;
    }
}

int AsyncCommand::getOperIndex()
{
    return m_index;
}

void  AsyncCommand::setOperIndex(int v)
{
    m_index = v;
}

void AsyncCommand::undo()
{
    for (auto var : m_record)
    {
        m_operWin->undoSync(var);

        delete var;
    }

    m_record.clear();
}

QString AsyncCommand::desc()
{
    return m_desc;
}

void AsyncCommand::setDesc(QString v)
{
    m_desc = v;
}

void AsyncCommand::addRecord(OperatorRecord* v)
{
    m_record.push_front(v);
}
