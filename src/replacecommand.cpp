#include "replacecommand.h"
#include "command.h"
#include "comparewin.h"

ReplaceCommand::ReplaceCommand(CompareWin* operWin) :m_operWin(operWin), m_index(0)
{
}

ReplaceCommand::~ReplaceCommand()
{
    for (auto var : m_record)
    {
        delete var;
    }
}

int ReplaceCommand::getOperIndex()
{
    return m_index;
}

void ReplaceCommand::undo()
{
    for (auto var : m_record)
    {
        m_operWin->undoReplace(var);

        delete var;
    }

    m_record.clear();
}

QString ReplaceCommand::desc()
{
    return m_desc;
}

void ReplaceCommand::setDesc(QString v)
{
    m_desc = v;
}

void ReplaceCommand::addRecord(ReplaceOperRecords* v)
{
    m_record.push_front(v);
}

void ReplaceCommand::setOperIndex(int v)
{
    m_index = v;
}
