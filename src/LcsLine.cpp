#include "LcsLine.h"
#include <string.h>
#include <QStack> 

//该算法完全来自《A Linear Space Algorithm for Computing Maximal Common Subsequences》（D.S.Hirschberg著）
//参考https://blog.csdn.net/pkrobbie/article/details/1818477
//参考https://blog.csdn.net/weixin_34050389/article/details/85477029

#ifdef _DEBUG
static int cmp_times = 0;
static int new_times = 0;
static int delete_times = 0;
static int new_mem = 0;
static int delete_mem = 0;
#endif

static const int LARGE_FILE_LINENUM = 3000;


LcsLine::LcsLine(QByteArray * strA, QByteArray * strB, int lenA, int lenB):m_isCancel(nullptr), QObject(nullptr), m_curStep(0),m_leftStep(0)
{
	m_lenA = lenA;
	m_lenB = lenB;

	m_strA = strA;
	m_strB = strB;
}

//返回字符串的Lcs长度
static const qint64 MAX_CMP_LIMIT = 10000 * 10000;//每行最大1万个字符。否则不对比

int LcsLine::getLcsLength(int m, int n, QByteArray* strA, QByteArray* strB)
{
	if (m <= 0 || n <= 0)
	{
		return 0;
	}

	//加个长行保护，如果行太长，也直接认定为不等。否则出不来。可能溢出<0
	qint64 totalSize = m * n;

	if ((totalSize > MAX_CMP_LIMIT) || (totalSize < 0))
	{
		return 0;
	}

	int* K[2] = { nullptr, nullptr };

	K[0] = new int[n + 1];
	K[1] = new int[n + 1];


#ifdef _DEBUG
	new_mem += 2 * (n + 1);
	new_times += 2;
#endif

	memset(K[1], 0, (n + 1) * sizeof(int));

	for (int i = 0; i < m; ++i)
	{
		if (isCancel())
		{
			break;
		}
		memcpy(K[0], K[1], (n + 1) * sizeof(int));

		for (int j = 0; j < n; ++j)
		{
			if (strA[i] == strB[j])
			{
				K[1][j + 1] = K[0][j] + 1;
			}
			else
			{
				if (K[1][j] >= K[0][j + 1])
				{
					K[1][j + 1] = K[1][j];
				}
				else
				{
					K[1][j + 1] = K[0][j + 1];
				}
			}
		}
	}

	delete[] K[0];

#ifdef _DEBUG
	delete_times++;
	delete_mem += (n + 1);
#endif
	int ret = K[1][n];

	delete[] K[1];
	return ret;
}


//Reverse 反向还是正向比较
int * LcsLine::findRow(int m, int n, QByteArray *strA, QByteArray *strB, bool Reverse)
{
	int * K[2] = { nullptr, nullptr };

	K[0] = new int[n + 1];
	K[1] = new int[n + 1];


#ifdef _DEBUG
	new_mem += 2 * (n + 1);
	new_times += 2;
#endif

	memset(K[1], 0, (n + 1) * sizeof(int));

	for (int i = 1; i <= m; ++i)
	{
		if (isCancel())
		{
			break;
		}
		//大文件对比时，每次直接输出进度
		if ((m > LARGE_FILE_LINENUM || n > LARGE_FILE_LINENUM) && (i % 1000 == 0))
		{
			emit outMsg(1, tr("left Big Step %1, dealed %2 Step, compute %3 step, total %4 steps ...").arg(m_leftStep).arg(m_curStep).arg(i).arg(m));
		}

		memcpy(K[0], K[1], (n + 1) * sizeof(int));

		for (int j = 1; j <= n; ++j)
		{
			if ((!Reverse && strA[i-1] == strB[j-1]) || (Reverse && strA[-i+1] == strB[-j+1]))
			{
				K[1][j] = K[0][j-1] + 1;
			}
			else
			{
				if (K[1][j-1] >= K[0][j])
				{
					K[1][j] = K[1][j-1];
				}
				else
				{
					K[1][j] = K[0][j];
				}
			}
		}
	}

	delete[] K[0];

#ifdef _DEBUG
	delete_times++;
	delete_mem += (n + 1);
#endif

	return K[1];
}

//保存递归的每次栈上参数
typedef struct lcs_para_ {
	int m;
	int n;
	QByteArray *strA;
	QByteArray *strB;
	lcs_para_()
	{
		m = n = -1;
	}
	lcs_para_(int m_, int n_, QByteArray *strA_, QByteArray *strB_)
	{
		m = m_;
		n = n_;
		strA = strA_;
		strB = strB_;
	}
}LCS_PARA;

//保存前后两次的结构
typedef struct lcs_result_ {
	QByteArray *ret;
	int length;
	bool needRelease;
	lcs_result_()
	{
		length = 0;
	}
	lcs_result_(QByteArray *ret_, int length_, bool needRelease_ = true)
	{
		ret = ret_;
		length = length_;
		needRelease = needRelease_;
	}
}LCS_RESULT;

#ifdef _DEBUG
//该函数是递归版本。
QByteArray * LcsLine::H_LCS(int m, int n, QByteArray * strA, QByteArray *strB, int &reLen)
{
#ifdef _DEBUG
	cmp_times++;
#endif

	QByteArray * result = nullptr;
	reLen = 0;

	if (0 == n)
	{
		return result;
	}
	else if (1 == m)
	{
		if (find(strB, strB + n, strA[0]))
		{
			result = new QByteArray[1];
			*result = strA[0];
			reLen = 1;
#ifdef _DEBUG
			new_times++;
			new_mem++;
#endif
			return result;
		}
		else
		{
			return nullptr;
		}
	}
	else
	{
		int i = m / 2;

		int * L1 = findRow(i, n, strA, strB);

		int *L2 = findRow(m - i, n, strA + m - 1, strB + n - 1, true);

		int M = 0;
		int k = 0;

		for (int j = 0; j <= n; ++j)
		{
			int tmp = L1[j] + L2[n - j];
			if (tmp > M)
			{
				M = tmp;
				k = j;
			}
		}
		delete[] L1;
		delete[] L2;

#ifdef _DEBUG
		delete_times += 2;
		delete_mem += 2 * (n + 1);
#endif

		int r1 = 0;
		int r2 = 0;

		QByteArray * C1 = nullptr;
		QByteArray * C2 = nullptr;


		C1 = H_LCS(i, k, strA, strB, r1);

		C2 = H_LCS(m - i, n - k, strA + i, strB + k, r2);

		if (r1 + r2 > 0)
		{
			result = new QByteArray[r1 + r2];
#ifdef _DEBUG
			new_times++;
			new_mem += (r1 + r2);
#endif
			if (r1 > 0)
			{
				//memcpy(result, C1, r1);
				for (int i = 0; i < r1; ++i)
				{
					result[i] = C1[i];
				}
				if (r1 > 0)
				{
					delete[] C1;
#ifdef _DEBUG
					delete_times++;
					delete_mem += r1;
#endif
				}
			}
			if (r2 > 0)
			{
				//memcpy(result + r1, C2, r2);
				for (int i = 0; i < r2; ++i)
				{
					result[r1+i] = C2[i];
				}
				if (r2 > 0)
				{
					delete[] C2;
#ifdef _DEBUG
					delete_times++;
					delete_mem += r2;
#endif
				}
			}

			reLen = r1 + r2;
		}
		else
		{
			return nullptr;
		}
	}
	return result;
}
#endif


//非递归版本。建议使用非递归版本，因为windows 栈可能只有2M，会溢出
QByteArray * LcsLine::H_LCS1(int m, int n, QByteArray * strA, QByteArray *strB, int &length)
{

	QByteArray * result = nullptr;
	length = 0;

	//用户递归调用的栈
	QStack<LCS_PARA> stack;

	//用于保存结果的栈
	QStack<LCS_RESULT> resultStack;

	LCS_PARA para_first(m, n, strA, strB);

	stack.push(para_first);

	while (!stack.isEmpty())
	{
		m_curStep++;
		m_leftStep = stack.size();

		//大文件对比时，每次直接输出进度
		if (m > LARGE_FILE_LINENUM || n > LARGE_FILE_LINENUM)
		{
			emit outMsg(1, tr("Compare loop %1 times, %2 times left ...").arg(m_curStep).arg(m_leftStep));
		}

#ifdef _DEBUG
		cmp_times++;
#endif
		if (isCancel())
		{
			stack.clear();
			break;
		}

		LCS_PARA para_t = stack.pop();

		if (0 == para_t.n)
		{
			LCS_RESULT result_null(nullptr, 0, false);
			resultStack.push(result_null);
		}
		else if (1 == para_t.m)
		{
			if (find(para_t.strB, para_t.strB + para_t.n, para_t.strA[0]))
			{
				LCS_RESULT result_one(para_t.strA, 1, false);
				resultStack.push(result_one);
			}
			else
			{
				LCS_RESULT result_null(nullptr, 0, false);
				resultStack.push(result_null);
			}
		}
		else
		{
			int i = para_t.m / 2;

			int * L1 = findRow(i, para_t.n, para_t.strA, para_t.strB);

			int *L2 = findRow(para_t.m - i, para_t.n, para_t.strA + para_t.m - 1, para_t.strB + para_t.n - 1, true);

			int M = 0;
			int k = 0;

			for (int j = 0; j <= para_t.n; ++j)
			{
				int tmp = L1[j] + L2[para_t.n - j];
				if (tmp > M)
				{
					M = tmp;
					k = j;
				}
			}
			delete[] L1;
			delete[] L2;

#ifdef _DEBUG
			delete_times += 2;
			delete_mem += 2 * (para_t.n + 1);
#endif

			//注意递归版本是先求C1 再C2,压栈顺序要相反，后进去的反而先执行
			//C2 = H_LCS(m - i, n - k, strA + i, strB + k, r2);
			LCS_PARA para_t2(para_t.m - i, para_t.n - k, para_t.strA + i, para_t.strB + k);
			stack.push(para_t2);

			//其实下次处理的反而是这个
			//C1 = H_LCS(i, k, strA, strB, r1);
			LCS_PARA para_t1(i, k, para_t.strA, para_t.strB);
			stack.push(para_t1);
		}


		//有结果就及时合并处理
		//最开始这个是放在上面的while进来处，发现while退出时，会漏掉最后一个结果。故需要放在这里
		if (resultStack.count() >= 2)
		{

			LCS_RESULT r2 = resultStack.pop();
			LCS_RESULT r1 = resultStack.pop();

#ifdef _DEBUG
			if (r1.length == 0 || r2.length == 0)
			{
				//qDebug(">>>1 %d %d",r1.length, r2.length);
			}
#endif

			if (r1.length + r2.length > 0)
			{
				
				if (r1.length > 0 && r2.length >0)
				{
					result = new QByteArray[r1.length + r2.length];
#ifdef _DEBUG
					new_times++;
					new_mem += (r1.length + r2.length);
#endif
					//不能调用memcpy进行拷贝，因为不会调用重载=函数
					//memcpy(result, r1.ret, r1.length*sizeof(QByteArray));
					for (int i = 0; i < r1.length; ++i)
					{
						result[i] = r1.ret[i];
					}

					if (r1.needRelease)
					{
						delete[] r1.ret;
#ifdef _DEBUG
						delete_times++;
						delete_mem += r1.length;
#endif
					}

					//memcpy(result + r1.length, r2.ret, r2.length* sizeof(QByteArray));
					for (int i = 0; i < r2.length; ++i)
					{
						result[r1.length + i] = r2.ret[i];
					}

					if (r2.needRelease)
					{
						delete[] r2.ret;
#ifdef _DEBUG
						delete_times++;
						delete_mem += r2.length;
#endif
					}

					LCS_RESULT result_one(result, r1.length + r2.length);
					resultStack.push(result_one);
				}
				else if (r1.length > 0)
				{
					LCS_RESULT result_one(r1.ret, r1.length,r1.needRelease);
					resultStack.push(result_one);
				}
				else if (r2.length > 0)
				{
					LCS_RESULT result_one(r2.ret, r2.length, r2.needRelease);
					resultStack.push(result_one);
				}
			}
		}
	}

	//最后退出时，结果就在最后的resultStack中
	if (resultStack.count() > 0)
	{
		LCS_RESULT r = resultStack.pop();
		length = r.length;

		//如果不需要释放内存，则拷贝一份。因为内存统一在外面是要释放的
		//否则可能释放了不该释放的区域
		if (r.length > 0 && !r.needRelease)
		{
			//不需要释放，说明占用的是不能释放的，直接拷贝一份到外面
			result = new QByteArray[r.length];
#ifdef _DEBUG
			new_times++;
			new_mem += (r.length);
#endif
			for (int i = 0; i < r.length; ++i)
			{
				result[i] = r.ret[i];
			}
		}
	}


	return result;
}


//从begin 开始查找，一直到end结束 [begin,end),不包含end
bool LcsLine::find(QByteArray * begin, QByteArray * end, QByteArray d)
{
	while (begin != end)
	{
		if (*begin == d)
		{
			return true;
		}
		++begin;
	}
	return false;
}

QByteArray* LcsLine::cmp(int &lens)
{
	//做个保护，如果是明确的空，直接返回
	if (m_lenA == 0 || m_lenB == 0)
	{
		lens = 0;
		return nullptr;
	}

	QByteArray* ret = H_LCS1(m_lenA, m_lenB, m_strA, m_strB, lens);

#ifdef _DEBUG
	//qDebug("new_times %d delete_times %d cmp_times %d new_mem %d delete_mem %d return no release %d \n", new_times, delete_times, cmp_times, new_mem, delete_mem, lens);
#endif
	return ret;
}

LcsLine::~LcsLine()
{
	m_isCancel = nullptr;
}

