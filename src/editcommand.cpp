#include "editcommand.h"
#include "command.h"
#include "comparewin.h"

EditCommand::EditCommand(CompareWin* operWin) :m_operWin(operWin), m_index(0)
{
}

EditCommand::~EditCommand()
{
    for (auto var : m_record)
    {
        delete var;
    }
}

int EditCommand::getOperIndex()
{
    return m_index;
}

void EditCommand::undo()
{
    for (auto var : m_record)
    {
        m_operWin->undoEdit(var);

        delete var;
    }

    m_record.clear();
}

QString EditCommand::desc()
{
    return m_desc;
}

void EditCommand::addRecord(ModifyOperRecords* v)
{
    m_record.push_front(v);
}

void EditCommand::setOperIndex(int v)
{
    m_index = v;
}

void EditCommand::setDesc(QString v)
{
    m_desc = v;
}
