#pragma once
#include <QtGlobal>

template<typename T>
class LcsTemplate
{
public:
	LcsTemplate(T * strA, T * strB, int lenA, int lenB);
	virtual ~LcsTemplate();
	static int getLcsLength(int m, int n, T * strA, T * strB);
	T* cmp(int &lens);

private:

	int * findRow(int m, int n, T *strA, T *strB, bool Reverse = false);

	T *H_LCS1(int m, int n, T * strA, T *strB, int &reLen);

	bool find(T* begin, T *end, T d);

	int m_lenA;
	int m_lenB;

	T *m_strA;
	T *m_strB;
};

#include "LcsTemplate.h"

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

template<typename T>
LcsTemplate<T>::LcsTemplate(T * strA, T * strB, int lenA, int lenB)
{
	m_lenA = lenA;
	m_lenB = lenB;

	m_strA = strA;
	m_strB = strB;
}
const qint64 MAX_CMP_LIMIT = 10000 * 10000;//每行最大1万个字符。否则不对比

//返回字符串的Lcs长度
template<typename T>
int LcsTemplate<T>::getLcsLength(int m, int n, T *strA, T *strB)
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
template<typename T>
int * LcsTemplate<T>::findRow(int m, int n, T *strA, T *strB, bool Reverse)
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
		memcpy(K[0], K[1], (n + 1) * sizeof(int));

		for (int j = 1; j <= n; ++j)
		{
			if ((!Reverse && strA[i - 1] == strB[j - 1]) || (Reverse && strA[-i + 1] == strB[-j + 1]))
			{
				K[1][j] = K[0][j - 1] + 1;
			}
			else
			{
				if (K[1][j - 1] >= K[0][j])
				{
					K[1][j] = K[1][j - 1];
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
template<typename T>
struct lcs_para_tpl {
	int m;
	int n;
	T *strA;
	T *strB;
	lcs_para_tpl()
	{
		m = n = -1;
	}
	lcs_para_tpl(int m_, int n_, T *strA_, T *strB_)
	{
		m = m_;
		n = n_;
		strA = strA_;
		strB = strB_;
	}
};

//保存前后两次的结构
template<typename T>
struct lcs_result_tpl {
	T *ret;
	int length;
	bool needRelease;
	lcs_result_tpl()
	{
		length = 0;
	}
	lcs_result_tpl(T *ret_, int length_, bool needRelease_ = true)
	{
		ret = ret_;
		length = length_;
		needRelease = needRelease_;
	}
};


//非递归版本。建议使用非递归版本，因为windows 栈可能只有2M，会溢出
template<typename T>
T * LcsTemplate<T>::H_LCS1(int m, int n, T * strA, T *strB, int &length)
{

	T * result = nullptr;
	length = 0;

	//用户递归调用的栈
	QStack<lcs_para_tpl<T> > stack;

	//用于保存结果的栈
	QStack<lcs_result_tpl<T> > resultStack;

	lcs_para_tpl<T>  para_first(m, n, strA, strB);

	stack.push(para_first);

	while (!stack.isEmpty())
	{
#ifdef _DEBUG
		cmp_times++;
#endif

		lcs_para_tpl<T>  para_t = stack.pop();

		if (0 == para_t.n)
		{
			lcs_result_tpl<T>  result_null(nullptr, 0, false);
			resultStack.push(result_null);
		}
		else if (1 == para_t.m)
		{
			if (find(para_t.strB, para_t.strB + para_t.n, para_t.strA[0]))
			{
				lcs_result_tpl<T>  result_one(para_t.strA, 1, false);
				resultStack.push(result_one);
			}
			else
			{
				lcs_result_tpl<T>  result_null(nullptr, 0, false);
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
			lcs_para_tpl<T>  para_t2(para_t.m - i, para_t.n - k, para_t.strA + i, para_t.strB + k);
			stack.push(para_t2);

			//其实下次处理的反而是这个
			//C1 = H_LCS(i, k, strA, strB, r1);
			lcs_para_tpl<T>  para_t1(i, k, para_t.strA, para_t.strB);
			stack.push(para_t1);
		}


		//有结果就及时合并处理
		//最开始这个是放在上面的while进来处，发现while退出时，会漏掉最后一个结果。故需要放在这里
		if (resultStack.count() >= 2)
		{

			lcs_result_tpl<T>  r2 = resultStack.pop();
			lcs_result_tpl<T>  r1 = resultStack.pop();

#ifdef _DEBUG
			if (r1.length == 0 || r2.length == 0)
			{
				//qDebug(">>>1 %d %d",r1.length, r2.length);
			}
#endif

			if (r1.length + r2.length > 0)
			{

				if (r1.length > 0 && r2.length > 0)
				{
					result = new T[r1.length + r2.length];
#ifdef _DEBUG
					new_times++;
					new_mem += (r1.length + r2.length);
#endif
					//不能调用memcpy进行拷贝，因为不会调用重载=函数
					//memcpy(result, r1.ret, r1.length*sizeof(T));
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

					//memcpy(result + r1.length, r2.ret, r2.length* sizeof(T));
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

					lcs_result_tpl<T>  result_one(result, r1.length + r2.length);
					resultStack.push(result_one);
				}
				else if (r1.length > 0)
				{
					lcs_result_tpl<T>  result_one(r1.ret, r1.length, r1.needRelease);
					resultStack.push(result_one);
				}
				else if (r2.length > 0)
				{
					lcs_result_tpl<T>  result_one(r2.ret, r2.length, r2.needRelease);
					resultStack.push(result_one);
				}
			}
		}
	}

	//最后退出时，结果就在最后的resultStack中
	if (resultStack.count() > 0)
	{
		lcs_result_tpl<T>  r = resultStack.pop();
		length = r.length;

		//如果不需要释放内存，则拷贝一份。因为内存统一在外面是要释放的
		//否则可能释放了不该释放的区域
		if (r.length > 0 && !r.needRelease)
		{
			//不需要释放，说明占用的是不能释放的，直接拷贝一份到外面
			result = new T[r.length];
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
template<typename T>
bool LcsTemplate<T>::find(T * begin, T * end, T d)
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

template<typename T>
T* LcsTemplate<T>::cmp(int &lens)
{
	if (m_lenA == 0 || m_lenB == 0)
	{
		lens = 0;
		return nullptr;
	}

	T* ret = H_LCS1(m_lenA, m_lenB, m_strA, m_strB, lens);

#ifdef _DEBUG
	//qDebug("new_times %d delete_times %d cmp_times %d new_mem %d delete_mem %d return no release %d \n", new_times, delete_times, cmp_times, new_mem, delete_mem, lens);
#endif
	return ret;
}

template<typename T>
LcsTemplate<T>::~LcsTemplate()
{
}

