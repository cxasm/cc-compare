#include "Lcs.h"
#include <string.h>
#include <QStack> 
#if defined(Q_OS_MAC)
#else
#include <omp.h>
#endif
#include <QThread>

//该算法完全来自《A Linear Space Algorithm for Computing Maximal Common Subsequences》（D.S.Hirschberg著）
//参考https://blog.csdn.net/pkrobbie/article/details/1818477
//参考https://blog.csdn.net/weixin_34050389/article/details/85477029

#ifdef _DEBUG
int cmp_times = 0;
int new_times = 0;
int delete_times = 0;
int new_mem = 0;
int delete_mem = 0;
#endif



Lcs::Lcs(uchar * strA, uchar * strB, int lenA, int lenB):m_binLcsResult(nullptr)
{
	m_lenA = lenA;
	m_lenB = lenB;

	m_strA = strA;
	m_strB = strB;
}

//返回字符串的Lcs长度
int Lcs::getLcsLength(int m, int n, uchar *strA, uchar *strB)
{
	int * K[2] = { nullptr, nullptr };

	K[0] = new int[n + 1];
	K[1] = new int[n + 1];


#ifdef _DEBUG
	new_mem += 2 * (n + 1);
	new_times += 2;
#endif

	memset(K[1], 0, (n + 1) * sizeof(int));

	for (int i = 0; i < m; ++i)
	{
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
int * Lcs::findRow(int m, int n, uchar *strA, uchar *strB, bool Reverse)
{
	int * K[2] = { nullptr, nullptr };

	K[0] = new int[n + 1];
	K[1] = new int[n + 1];


#ifdef _DEBUG
	new_mem += 2 * (n + 1);
	new_times += 2;
#endif

	memset(K[1], 0, (n + 1) * sizeof(int));

	for (int i = 0; i < m; ++i)
	{
		if ((m_binLcsResult != nullptr) && m_binLcsResult->isCmpCancel)
		{
			break;
		}

		memcpy(K[0], K[1], (n + 1) * sizeof(int));

		for (int j = 0; j < n; ++j)
		{
			if ((!Reverse && strA[i] == strB[j]) || (Reverse && strA[-i] == strB[-j]))
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

	return K[1];
}

//保存递归的每次栈上参数
typedef struct lcs_para_ {
	int m;
	int n;
	uchar *strA;
	uchar *strB;
	lcs_para_()
	{
		m = n = -1;
	}
	lcs_para_(int m_, int n_, uchar *strA_, uchar *strB_)
	{
		m = m_;
		n = n_;
		strA = strA_;
		strB = strB_;
	}
}LCS_PARA;

//保存前后两次的结构
typedef struct lcs_result_ {
	uchar *ret;
	int length;
	bool needRelease;
	lcs_result_()
	{
		length = 0;
	}
	lcs_result_(uchar *ret_, int length_, bool needRelease_ = true)
	{
		ret = ret_;
		length = length_;
		needRelease = needRelease_;
	}
}LCS_RESULT;

#ifdef _DEBUG
//该函数是递归版本。
uchar * Lcs::H_LCS(int m, int n, uchar * strA, uchar *strB, int &reLen)
{
#ifdef _DEBUG
	cmp_times++;
#endif

	uchar * result = nullptr;
	reLen = 0;

	if (0 == n)
	{
		return result;
	}
	else if (1 == m)
	{
		if (find(strB, strB + n, strA[0]))
		{
			result = new uchar[1];
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

		int *L2 = findRow(m - i, n, strA + m -1, strB + n -1, true);

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

		uchar * C1 = nullptr;
		uchar * C2 = nullptr;

	
		C1 = H_LCS(i, k, strA, strB, r1);
		
		C2 = H_LCS(m - i, n - k, strA + i, strB + k, r2);
		
		if (r1 + r2 > 0)
		{
			result = new uchar[r1 + r2];
#ifdef _DEBUG
			new_times++;
			new_mem += (r1 + r2);
#endif
			if (r1 > 0)
			{
				memcpy(result, C1, r1);
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
				memcpy(result + r1, C2, r2);
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
//返回值是指向LCS的字符串，length是其长度
uchar * Lcs::H_LCS1(int m, int n, uchar * strA, uchar *strB, int &length)
{

	uchar * result = nullptr;
	length = 0;

	//用户递归调用的栈
	QStack<LCS_PARA> stack;

	//用于保存结果的栈
	QStack<LCS_RESULT> resultStack;

	LCS_PARA para_first(m,n,strA,strB);

	stack.push(para_first);

	int loopTimes = 0;

	while (!stack.isEmpty())
	{
#ifdef _DEBUG
		cmp_times++;
#endif
		
		if (loopTimes % 1000 == 0)
		{
			emit reportStep(0, QString("The comparison is in progress, currently %1 iterations.").arg(loopTimes));
		}

		loopTimes++;

		LCS_PARA para_t = stack.pop();

		//如果取消，直接返回？20211209添加，如果不稳定，可以后续删除
		if ((m_binLcsResult != nullptr) && m_binLcsResult->isCmpCancel)
		{
			continue;
		}

		if (0 == para_t.n)
		{
			LCS_RESULT result_null(nullptr,0,false);
			resultStack.push(result_null);
		}
		else if (1 == para_t.m)
		{
			if (find(para_t.strB, para_t.strB + para_t.n, para_t.strA[0]))
			{
				LCS_RESULT result_one(para_t.strA, 1,false);
				resultStack.push(result_one);
			}
			else
			{
				LCS_RESULT result_null(nullptr, 0,false);
				resultStack.push(result_null);
			}
		}
		else
		{
			int i = para_t.m / 2;

			int * L1 = findRow(i, para_t.n, para_t.strA, para_t.strB);

			int *L2 = findRow(para_t.m - i, para_t.n, para_t.strA + para_t.m -1, para_t.strB + para_t.n -1, true);

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
					result = new uchar[r1.length + r2.length];
#ifdef _DEBUG
					new_times++;
					new_mem += (r1.length + r2.length);
#endif
					
					memcpy(result, r1.ret, r1.length*sizeof(uchar));
				

					if (r1.needRelease)
					{
						delete[] r1.ret;
#ifdef _DEBUG
						delete_times++;
						delete_mem += r1.length;
#endif
					}

					memcpy(result + r1.length, r2.ret, r2.length* sizeof(uchar));
				

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

	if (resultStack.count() > 0)
	{
		//最后退出时，结果就在最后的resultStack中
		LCS_RESULT r = resultStack.pop();
		length = r.length;

		//如果不需要是否内存，则拷贝一份。因为内存统一在外面是要释放的
		//否则可能释放了不该释放的区域

		if (r.length > 0 && !r.needRelease)
		{
			//不需要释放，说明占用的是不能释放的，直接拷贝一份到外面
			result = new uchar[r.length];
			memcpy(result, r.ret, r.length);
		}
	}

	return result;
}


//从begin 开始查找，一直到end结束 [begin,end),不包含end
bool Lcs::find(uchar * begin, uchar * end, uchar d)
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

uchar* Lcs::cmp(int &lens)
{
	uchar* ret = H_LCS1(m_lenA, m_lenB, m_strA, m_strB, lens);

#ifdef _DEBUG
	qDebug("uchar cmp new_times %d delete_times %d cmp_times %d new_mem %d delete_mem %d return no release %d \n", new_times, delete_times, cmp_times, new_mem, delete_mem, lens);
#endif
	return ret;
}


//返回对比的步数。主要在分块对比时，返回分块后的块数
int Lcs::getCmpTotalStep(int leftSize, int rightSize)
{
	int minLen = (leftSize >= rightSize) ? rightSize : leftSize;
	int times = minLen / 8192 + 1;

	return times;
}

//分块对比,每次对比blockSize的大小。最好是16的整数倍。16*512,每次8k
//把分块对比的lcs结构输出到容器中
void Lcs::cmp(int blockSize, QVector<uchar*>& lcsDatas, QVector<int>& lcsSize)
{
	int minLen = (m_lenA >= m_lenB) ? m_lenB : m_lenA;
	int times = minLen / blockSize;

	if (times > 0)
	{
		int *pSize = new int[times];
		uchar** pLcsRet = new uchar*[times];

		//每次blockSize来对比文件

		int finishTicks = 0;

		QThread* pMainThd = this->thread();

		#pragma omp parallel for
		for (int i = 0; i < times; ++i)
		{
			//emit reportStep(0, QString("The comparison is in progress, currently %1 iterations.").arg(i));
			pLcsRet[i] = H_LCS1(blockSize, blockSize, m_strA + i * blockSize, m_strB + i * blockSize, pSize[i]);
			finishTicks++;

			if (pMainThd == QThread::currentThread())
			{
				emit reportStep(3, QString("%1").arg(finishTicks));
			}

		}

		for (int i = 0; i < times; ++i)
		{
			lcsDatas.append(pLcsRet[i]);
			lcsSize.append(pSize[i]);
		}

		delete[]pSize;
		delete[]pLcsRet;
	}

	//尾巴上还有一次
	if ((minLen % blockSize) > 0)
	{
		int lens = 0;
	
		int remainA = m_lenA - times * blockSize;
		int remainB = m_lenB - times * blockSize;

		//20211218发现一个问题，如果只取长的末尾的blockSize，对比结果可能会不全。所以长的需要取2倍，避免不等的部分太明显
		if (remainA > 2*blockSize)
		{
			remainA = 2 * blockSize;
		}
		if (remainB > blockSize)
		{
			remainB = 2 * blockSize;
		}
		uchar* ret = H_LCS1(remainA, remainB, m_strA + times * blockSize, m_strB + times * blockSize, lens);

		lcsDatas.append(ret);
		lcsSize.append(lens);

		emit reportStep(1, QString());
	}

	emit reportStep(2, QString());
}

void Lcs::setParameter(BinLcsResult * para)
{
	m_binLcsResult = para;
}

Lcs::~Lcs()
{
}
