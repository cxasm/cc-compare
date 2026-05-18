#include "rcglobal.h"
#include <QProcess>
#include <QTextCodec>


const int M_SIZE = 1024 * 1024;
const int G_SIZE = 1024 * 1024 * 1024;

//把字节大小文件，转换为M 或 G 单位
QString tranFileSize(qint64 fileSize)
{
	float num = 0.0f;
	QString unit;

	if (fileSize >= G_SIZE)
	{
		num = double(fileSize) / G_SIZE;
		unit = "GB";
	}
	else if (fileSize >= M_SIZE)
	{
		num = double(fileSize) / M_SIZE;
		unit = "MB";
	}
	else if (fileSize > 1024)
	{
		num = float(fileSize) / 1024;
		unit = "KB";
	}
	else
	{
		return QString("%1").arg(fileSize);
	}

	return QString("%1 %2").arg(num, 0, 'f' , 2).arg(unit);
}

void showFileInExplorer(QString path)
{
	QString cmd;

#ifdef _WIN32
	path = path.replace("/", "\\");
	cmd = QString("explorer.exe /select,%1").arg(path);
#endif

#ifdef ubu
	path = path.replace("\\", "/");
	cmd = QString("nautilus %1").arg(path);
#endif

#ifdef uos
	path = path.replace("\\", "/");
	cmd = QString("dde-file-manager %1").arg(path);
#endif 

#if defined(Q_OS_MAC)
	path = path.replace("\\", "/");
	cmd = QString("open -R %1").arg(path);
#endif

	QProcess process;
	process.startDetached(cmd);
}

QTextCodec* getTextCodeByCode(CODE_ID dstCode)
{
	QTextCodec* pTranCode = nullptr;

	//如果编码是已知如下类型，则后续保存其它行时，不修改编码格式，继续按照原编码进行保存
	switch (dstCode)
	{
	case CODE_ID::UNICODE_BE:
	{
		pTranCode = QTextCodec::codecForName("UTF-16BE");
	}
	break;

	case  CODE_ID::UNICODE_LE:
	{
		pTranCode = QTextCodec::codecForName("UTF-16LE");
	}
	break;

	case CODE_ID::UTF8_BOM:
	{
		pTranCode = QTextCodec::codecForName("UTF-8");
	}
	break;

	case  CODE_ID::GBK:
	{
		pTranCode = QTextCodec::codecForName("GBK");
	}
	break;

	case  CODE_ID::BIG5:
	{
		pTranCode = QTextCodec::codecForName("BIG5-HKSCS");
	}
	break;

	case  CODE_ID::Shift_JIS:
	{
		pTranCode = QTextCodec::codecForName("Shift-JIS");
	}
	break;

	default:
		//对于其它非识别编码，统一转换为utf8。减去让用户选择的麻烦
		pTranCode = QTextCodec::codecForName("UTF-8");

	}
	return pTranCode;
}
