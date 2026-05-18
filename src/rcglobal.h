#pragma once

#include <QString>
#include <QVector>
#include <QTreeWidgetItem>
#include <qscilexer.h>
#include <QFileInfo>

#define CMP_CODE_NOEQUAL

enum RC_DIRECTION
{
	RC_INIT = -1,
	RC_LEFT = 0,
	RC_RIGHT,
	RC_BOTH,
};

enum RC_CMP_RESULT
{
	RC_RESULT_EQUAL=0,
	RC_RESULT_NOEQUAL,
};

enum RC_FILE_TYPE
{
	RC_FILE = 1000,
	RC_DIR,
	RC_PAD_FILE
};

enum RC_ITEM_STATUS
{
	RC_COLLAPSED = 0,
	RC_EXPANDED,
};

enum BLOCKSTATUS {
	UNKNOWN_BLOCK = 0,
	EQUAL_BLOCK = 1,
	UNEQUAL_BLOCK,
	PAD_BLOCK,
	LAST_PAD_EMPTY_BLOCK, // 最后一个用于对齐的空行
	TEMP_INSERT_BLOCK
};

/* 是放置在block中的userState，-1是保留行，因为-1是默认没有时的值，标识新插入行 */
enum RC_LINE_FORM
{
	PAD_LINE = -2,//对齐行 
	//-1没有使用QTextBlock::userState()的默认值
	UNKNOWN_LINE =0, //未知就是没有换行符号。当做没有
	UNIX_LINE,
	DOS_LINE,
	MAC_LINE,
};

typedef struct equalLineInfo_ {
	int index;//相等行序号，左右值
	int leftLineNums;
	int rightLineNums;
}EqualLineInfo;


typedef struct noequalblock_ {

	int startBlockNums; //开始的块号。左右两边都是一样的
	int blockLens;//左右长度。理论上二者是相等的,故只需要一个
	bool isDeleted; //是否已经被同步。即被删除过了，下次跳过这个
	int type; //
	noequalblock_()
	{
		type = UNKNOWN_BLOCK;
	}
	noequalblock_(int start, int lens)
	{
		startBlockNums = start;
		blockLens = lens;
		isDeleted = false;
		type = UNKNOWN_BLOCK;
	}
	bool operator==(const noequalblock_& other) const
	{
		return (startBlockNums == other.startBlockNums);
	}
}NoEqualBlock;

//必须和comparewin的底下编码保存一致
enum CODE_ID {
	IGNORE_HEX = -3,//忽略
	UNKOWN = -2,//其实应该是ANSI中的非GBK编码。暂时不考虑其它国家语言编码，则直接按照ASCII进行字节处理
	ANSI = -1,
	UTF8_NOBOM,//如果是这种，其实需要确定到底椒UTF8 还是ANSI
	UTF8_BOM, //UTF8_WITH BOM
	UNICODE_LE,

	UNICODE_BE,
	GBK,
	//增加国际化的几种语言
	EUC_JP,//日本鬼子
	Shift_JIS,//日文另外一种
	EUC_KR,//韩国
	KOI8_R,//俄罗斯
	TSCII,//泰国
	TIS_620,//泰文
	BIG5,//繁体中文
	UNICODE_LE_NOBOM,
	WINDOWS1250,//win1250 20230721新增
	IBM866,//20230724 新增。qt没有865，只有866
	CODE_END //最后一个标志,在UI上是显示一个UNKNOWN，这是一个特殊
};

/*作用：这个类主要统计左右不同的块，给界面上的“上一部分”和“下一部分”来使用。
*/
typedef struct BlocksInfo_ {

public:
	BlocksInfo_()
	{
		startLine = 0;
		endLine = 0;
	}

	BlocksInfo_(bool equal_, int startLine_, int endLine_, int actualNums_)
	{
		equal = equal_;
		actualNums = actualNums_;
		startLine = startLine_;
		endLine = endLine_;
	}

public:
	bool equal;//相同true，不同false
	int actualNums;//实际数据行数
	int startLine; //起点块的行号码
	int endLine;//终点块，不包含此块
}BlocksInfo;



//每一小段的字符，主要是将相等和不等的字符段分开
//tailStatus 20230404新加。尾巴节如果是\r\n的情况，要归并到上一个节。此时要保留该尾巴节是否相等的状态。
typedef struct SectionNode_ {
	bool equal; //是否相等
	qint8 tailStatus; //0不是尾巴 1 是相等的尾巴 2 是不等的尾巴。只有在尾巴与前一节相等状态不一样时，才有值；否则都是0值
	qint8 tailLens;//最后不等尾巴的长度。
	QString text;
	SectionNode_():tailStatus(0), tailLens(0),equal(false)
	{

	}
	//QByteArray text;
}SectionNode;


//每一小段的二进制字节，主要是将相等和不等的二进制字符段分开
typedef struct BinSectionNode_ {
	bool equal; //是否相等
	QVector<uchar> bytes;
}BinSectionNode;

typedef struct BinUnequalPos_ {
	int start;
	int end;
}BinUnequalPos;

//每一行的数据结构。每一行包含许多相等或不相等的小段
typedef struct LineNode_ {
	int lineNums;//行的号码
	bool totalEqual;//是否完全相等
	QVector<SectionNode> lineText;
	LineNode_()
	{
		totalEqual = false;
	}
	void clear()
	{
		totalEqual = false;
		lineText.clear();
	}
}LineNode;

extern RC_LINE_FORM getLineEndType(QString line);

extern RC_LINE_FORM getLineEndType(const LineNode& lines);


//#ifdef Q_OS_UNIX
//extern QString loadFontFromFile(QString path,int code=0);
//#endif

typedef struct ModifyRecords_ {
	int position;//当前修改位置
	int modificationType;//1：增加 2 删除
	int length;//修改的长度
	int linesAdded;//增加多少行。正为增加，负数为减少
	bool isInPaste;//是否在拷贝中，在的话前面一个删除不能做处理，要等到后续添加消息
	ModifyRecords_(int position_, int type_, int length_, int linesAdded_) :position(position_), modificationType(type_), length(length_), linesAdded(linesAdded_)
	{
		isInPaste = false;
	}
}ModifyRecords;



typedef struct fileAttriNode_ {
	QString relativePath;//不带/而且不带最外层目录路径
	int type; //file or dirs，pad
	//int index; //用于表示先后顺序，用于向前向后的排序查找
	QTreeWidgetItem* parent; //父节点
	QTreeWidgetItem* selfItem; //如果是目录，则标记自己的节点
	fileAttriNode_()
	{
		parent = nullptr;
	}
	fileAttriNode_(QString relativePath_)
	{
		relativePath = relativePath_;
	}

	bool operator==(const fileAttriNode_& other) const
	{
		return (relativePath.compare(other.relativePath) == 0);
	}

}fileAttriNode;

struct CmpWalkFileInfo {
	int direction; //左右方向
	int type;//类型、文件、文件夹

	QString name; //文件名称，短名称。
	QFileInfo fileinfo;

	//以下两个只有文件才有
	QString modifyTime; //修改时间
	qint64 fileSize; //文件大小

	CmpWalkFileInfo(int dire_, QString name_, int type_) :direction(dire_), name(name_),type(type_)
	{

	}
};

struct WalkFileInfo {
	int direction;
	QTreeWidgetItem* root;
	QString path;
	WalkFileInfo(int dire_, QTreeWidgetItem* root_, QString path_) :direction(dire_), root(root_), path(path_)
	{

	}
};


const int MARGIN_NONE = 0;
const int MARGIN_SYNC_BT = 1;
const int MARGIN_SYNC_BT_BIT_MASK = 0x2;

const int MARGIN_VER_LINE = 2;
const int MARGIN_VER_LINE_BIT_MASK = 0x4;


const int MARGIN_LINE_NUM = 3;

enum WORK_STATUS
{
	FREE_STATUS = 0,
	CMP_WORKING
};


#define OPEN_UNDO_REDO 1

#ifdef OPEN_UNDO_REDO

class BlockUserData;

struct OperatorInfo {
	int startLineNums; //开始行号
	int lineLens;//左右长度。理论上二者是相等的,故只需要一个
	int type;
	QList<int> lineLength; //每一行的长度
	QList<char*> lineContents;// 每一行的内容
	QList<BlockUserData*> lineExternInfo; //每一行的额外信息
	NoEqualBlock noEqualBlockInfo;
	int noEqualindex;
};


enum OperRecordStatus {
	RC_OPER_SYNC = 1,//同步导致
	RC_OPER_EDIT,//编辑导致
};
const int Item_RelativePath = Qt::ToolTipRole;
const int Item_Index = Qt::UserRole + 1;
const int DIR_ITEM_MAXSIZE_FILE = Qt::UserRole + 2;
#endif


QString getUserLangDirPath();

//在这定义一次即可。
//#define uos 1

#ifdef Q_OS_WIN
#undef uos
#endif

#ifdef ubu
#undef uos
#endif

#ifdef uos
#undef ubu
#endif

#ifdef Q_OS_MAC
#undef uos
#endif

void showFileInExplorer(QString path);
QString tranFileSize(qint64 fileSize);
QTextCodec* getTextCodeByCode(CODE_ID dstCode);
