#include "RealCompare.h"
#include "cmpsql.h"

#include <QtWidgets/QApplication>

#include <QTextCodec>
#include <QTranslator>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <QSettings>
#include <QObject>
#include <QSharedMemory>
#include <QMessageBox>

#pragma comment(lib, "user32.lib")
#include <qt_windows.h>
const ULONG_PTR CUSTOM_TYPE = 10000;
const ULONG_PTR OPEN_CC_LEFT_FILE = 10001;
const ULONG_PTR OPEN_CC_RIGHT_FILE = 10002;
const ULONG_PTR OPEN_CC_LEFT_DIR = 10003;
const ULONG_PTR OPEN_CC_RIGHT_DIR = 10004;
const ULONG_PTR OPEN_NOTEPAD_TYPE = 10005;
#endif

#ifdef Q_OS_UNIX
#include <QStyleFactory>
#endif

#ifdef _WIN32
QSharedMemory shared("CCompare");

#if 0
void setToFileRightMenu()
{
	QString exepath = QCoreApplication::applicationFilePath();
	exepath = exepath.replace("/", "\\");
	QString iconTxt = exepath;
	QString exepathleft = exepath + " -left \"%1\"";
	QString exepathright = exepath + " -right \"%1\"";
	QString exepathleftdir = exepath + " -leftd \"%1\"";
	QString exepathrightdir = exepath + " -rightd \"%1\"";

	auto regeditSet = [](QString key, QString&exepath, QString& iconTxt)
	{
		QString menuDisplayName(key);
		QString keyPath = "HKEY_CLASSES_ROOT\\*\\shell\\" + menuDisplayName + "\\command";
		QString iconPath = "HKEY_CLASSES_ROOT\\*\\shell\\" + menuDisplayName;
		QSettings settings(keyPath, QSettings::NativeFormat);
		QSettings iconSettings(iconPath, QSettings::NativeFormat);

		if (settings.value(".").toString() != exepath)
		{
			settings.setValue(".", exepath);
			iconSettings.setValue("Icon", iconTxt);
		}
	};

	auto regeditDirSet = [](QString key, QString&exepath, QString& iconTxt)
	{
		QString menuDisplayName(key);
		QString keyPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\Directory\\shell\\" + menuDisplayName + "\\command";
		QString iconPath = "HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\Directory\\shell\\" + menuDisplayName;
		QSettings settings(keyPath, QSettings::NativeFormat);
		QSettings iconSettings(iconPath, QSettings::NativeFormat);

		if (settings.value(".").toString() != exepath)
		{
			settings.setValue(".", exepath);
			iconSettings.setValue("Icon", iconTxt);
		}
	};

	regeditSet(QObject::tr("Select Left Compare File"), exepathleft, iconTxt);
	regeditSet(QObject::tr("Select Right Compare File"), exepathright, iconTxt);

	regeditDirSet(QObject::tr("Select Left Compare Dir"), exepathleftdir, iconTxt);
	regeditDirSet(QObject::tr("Select Right Compare Dir"), exepathrightdir, iconTxt);
}
#endif

#endif

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	
#ifdef Q_OS_UNIX
    QApplication::setStyle(QStyleFactory::create("fusion"));
#endif

	QStringList arguments = QCoreApplication::arguments();

	RealCompare cmpwin;


	if (arguments.size() >= 3)
	{
#ifdef Q_OS_WIN
		auto sendMsgToMain = [](const QString& filename,int type) {
			qlonglong hwndId;
			shared.lock();
			memcpy(&hwndId, shared.data(), sizeof(qlonglong));
			shared.unlock();
			HWND hwnd = (HWND)hwndId;
			if (::IsWindow(hwnd))
			{
				QByteArray data = filename.toUtf8();

				COPYDATASTRUCT copydata;
				copydata.dwData = type; //自定义类型
				copydata.lpData = data.data();  //数据大小
				copydata.cbData = data.size();  // 指向数据的指针

				::SendMessage(hwnd, WM_COPYDATA, reinterpret_cast<WPARAM>(nullptr), reinterpret_cast<LPARAM>(&copydata));
			}
			return 0;
		};

		auto saveMainId = [&cmpwin]() {
			shared.create(32);
			memset((char*)shared.data(), 0, 32);

			cmpwin.showMinimized();
			qlonglong winId = (qlonglong)cmpwin.winId();
			shared.lock();
			memcpy((char*)shared.data(), &winId, sizeof(qlonglong));
			shared.unlock();
		};

		if (arguments[1] == "-left")
		{
			//attach成功表示已经存在该内存了，表示当前存在实例
			if (shared.attach())
			{
				sendMsgToMain(arguments[2], OPEN_CC_LEFT_FILE);
				return 0;
			}
			else
			{
				saveMainId();
				cmpwin.setLeftCmpFile(arguments[2]);
			}
		}
		else if (arguments[1] == "-right")
		{
			//attach成功表示已经存在该内存了，表示当前存在实例
			if (shared.attach())
			{
				sendMsgToMain(arguments[2], OPEN_CC_RIGHT_FILE);
				return 0;
			}
			else
			{
				saveMainId();
				cmpwin.setRightCmpFile(arguments[2]);
			}
		}
		else if (arguments[1] == "-leftd")
		{
			//attach成功表示已经存在该内存了，表示当前存在实例
			if (shared.attach())
			{
				sendMsgToMain(arguments[2], OPEN_CC_LEFT_DIR);
				return 0;
			}
			else
			{
				saveMainId();
				cmpwin.setLeftCmpDir(arguments[2]);
			}
		}
		else if (arguments[1] == "-rightd")
		{
			//attach成功表示已经存在该内存了，表示当前存在实例
			if (shared.attach())
			{
				sendMsgToMain(arguments[2], OPEN_CC_RIGHT_DIR);
				return 0;
			}
			else
			{
				saveMainId();
				cmpwin.setRightCmpDir(arguments[2]);
			}
		}
		else if (arguments.size() == 4 && arguments[1] == "-d")
		{
			//-d dir1 dir2的格式。
			cmpwin.compareDir(arguments[2], arguments[3]);
		}
		else
		{
			cmpwin.compareFile(arguments[1], arguments[2]);
		}
#else
		cmpwin.compareFile(arguments[1], arguments[2]);
#endif
	}
	else
	{
#ifdef Q_OS_WIN
		//attach成功表示已经存在该内存了，表示当前存在实例
		if (shared.attach())
		{
			//已经存在实例，不要新开实例
			//把窗口设置到最前
			qlonglong hwndId = 0;
			shared.lock();
			memcpy(&hwndId, shared.data(), sizeof(qlonglong));
			shared.unlock();

			HWND hwnd = (HWND)hwndId;
			if ((hwndId != 0) && ::IsWindow(hwnd))
			{
				int uTimeout = 4000;

				COPYDATASTRUCT copydata;
				copydata.dwData = OPEN_NOTEPAD_TYPE; //自定义类型，换出
				copydata.lpData = NULL;
				copydata.cbData = 0;

				LRESULT code = ::SendMessageTimeout(hwnd, WM_COPYDATA, reinterpret_cast<WPARAM>(nullptr), reinterpret_cast<LPARAM>(&copydata), SMTO_ABORTIFHUNG, uTimeout, nullptr);
				if (code != 1)
				{
					if (QMessageBox::Yes == QMessageBox::question(nullptr, u8"唤出实例失败", u8"前一个实例还在阻塞，或非法退出，是否打开新实例？"))
					{
						shared.detach();//这样释放一下，把之前的残留状态释放。
						shared.attach();

						goto new_instance;
					}
				}
				else
				{
					return 0;
				}
			}
			else
			{
				//不是一个合法的窗口，咋办。直接新开实例
				shared.detach();//这样释放一下，把之前的残留状态释放。
				shared.attach();
				goto new_instance;
			}
		}
			shared.create(32);

new_instance:
			memset((char*)shared.data(), 0, 32);
			cmpwin.show();

			qlonglong winId = (qlonglong)cmpwin.winId();
			shared.lock();
			memcpy((char*)shared.data(), &winId, sizeof(qlonglong));
			shared.unlock();

#endif


	}

	//setToFileRightMenu();

	return a.exec();
}
