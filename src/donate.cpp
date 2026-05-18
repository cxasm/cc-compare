#include "donate.h"
#include <QShortcut>

Donate::Donate(QWidget *parent): QWidget(parent)
{
	ui.setupUi(this);

	QShortcut* escSc = new QShortcut(this);
	escSc->setKey(QKeySequence(Qt::Key_Escape));
	escSc->setContext(Qt::WindowShortcut);
	connect(escSc, &QShortcut::activated, this, [this]() {
		close();
		});

#if defined(Q_OS_MAC)
    //必须这样，否则窗口总是跑到后面去
    Qt::WindowFlags m_flags = windowFlags();
    setWindowFlags(m_flags | Qt::Tool);
#endif
        #if defined(uos)
    Qt::WindowFlags m_flags = windowFlags();
    setWindowFlags(m_flags | Qt::Tool);
#endif
}

Donate::~Donate()
{

}
