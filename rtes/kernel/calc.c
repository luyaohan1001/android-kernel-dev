#include <linux/kernel.h>
#include <asm/uaccess.h> // strncpy_from_user, copy_to_user
#include <asm/string.h> // strlen

#define KPARAM_LENGTH 64

// asmlinkage tells your compiler to look on the CPU stack for the function parameters, instead of registers.

// return -1 for invalid, 0 for int, 1 for float
int parseInput(const char* kstr, int* output, int* decPos)
{
    int retVal;
    int i;
    int length;
    int tmp;
    retVal = 0;
    length = strlen(kstr);
    tmp = 0;
    *decPos = 0;
    for(i = 0; i < length; ++i)
    {
        if(kstr[i]<'0' || kstr[i]>'9')
        {
            if((retVal==0) && (kstr[i]=='.'))
            {
                *decPos = length - i - 1;
                retVal = 1;
                continue;
            }
            else
            {
                printk("SYSCALL_ERROR: invalid param %s.\n", kstr);
                return -1;
            }
        }
        else
        {
            tmp = tmp * 10 + kstr[i] - '0';
        }
    }
    *output = tmp;
    return retVal;
}

// return result without decimal point
int fpAddSub(char op, int in1, int decPos1, int in2, int decPos2, int* decPos)
{
    int i;
    *decPos = decPos1 > decPos2? decPos1 : decPos2;
    for(i=0; i<(*decPos-decPos1); ++i)
    {
        in1 *= 10;
    }
    for(i=0; i<(*decPos-decPos2); ++i)
    {
        in2 *= 10;
    }
    if(op=='-') return in1-in2;
    return in1+in2;
}

// return result without decimal point
int fpMult(int in1, int decPos1, int in2, int decPos2, int* decPos)
{
    *decPos = decPos1 + decPos2;
    return in1 * in2;
}

// return result without decimal point
int fpDiv(int in1, int decPos1, int in2, int decPos2, int* decPos)
{
    *decPos = decPos1 - decPos2 + 3;
    return in1*1000 / in2;
}

void printDec(char* kresult, int toPrint, int decPos)
{
    int i,j;
    int moveTen = 1;
    char intPart[KPARAM_LENGTH] = {};
    char decPart[KPARAM_LENGTH] = {};
    char zeroPadding[KPARAM_LENGTH] = {};
    for(i=0; i<decPos; ++i)
    {
        moveTen *= 10;
    }
    if(toPrint >= 0)
    {
        sprintf(intPart,"%d", toPrint/moveTen);
        sprintf(decPart,"%d", toPrint%moveTen);
    }
    else
    {
        sprintf(kresult,"-");
        sprintf(intPart,"%d", -toPrint/moveTen);
        sprintf(decPart,"%d", -toPrint%moveTen);
    }
    // check zero padding
    // NOTE strlen returns size_t (unsigned), may cause underflow
    for(i=0; i<(decPos-(int)strlen(decPart)); ++i)
    {
        zeroPadding[i] = '0';
    }
    sprintf(kresult, "%s.%s%s", intPart, zeroPadding, decPart);

    // Get rid of all trailing zeros when we have decimal part.
    for (j = (int)strlen(kresult)-1; j >= 0; j --) {
            // Stop when we have reached decimal point
            if (kresult[j-1] == '.') 
                break;
            // Insert Null character
            if (kresult[j] == '0') 
                kresult[j] = 0;
            else
                break;
    }
}


asmlinkage long sys_calc(const char* param1, const char* param2, char operation, char* result) {
    int errorCode = 0;
    int in1, in2;
    int decPos1, decPos2;
    int retVal1, retVal2;
    int out = 0;
    int decPosOut = 0;
    int moveTen = 1;
    int i;
    char kparam1[KPARAM_LENGTH] = {};
    char kparam2[KPARAM_LENGTH] = {};
    char kresult[KPARAM_LENGTH] = {};
    if(!param1 || !param2 || !result)
    {
        printk("SYSCALL_ERROR: sys_calc invalid pointer.\n");
        return EINVAL;
    }
    // copy the params from user
    if(!strncpy_from_user(kparam1, param1, KPARAM_LENGTH))
    {
        printk("SYSCALL_ERROR: sys_calc failed to get param1.\n");
        return EINVAL;
    }
    if(!strncpy_from_user(kparam2, param2, KPARAM_LENGTH))
    {
        printk("SYSCALL_ERROR: sys_calc failed to get param2.\n");
        return EINVAL;
    }
    // convert strings to fixed point
    // check for digit point
    retVal1 = parseInput(kparam1, &in1, &decPos1);
    retVal2 = parseInput(kparam2, &in2, &decPos2);
    if((retVal1==-1) || (retVal2==-1)) return EINVAL;
    if ((retVal1==0) && (retVal2==0))
    {
        switch(operation) {
            case '-':
                out = in1 - in2;
                break;
            case '+':
                out = in1 + in2;
                break;
            case '*':
                out = in1 * in2;
                break;
            case '/':
                if(!in2)
                {
                    printk("SYSCALL_ERROR: sys_calc devided by zero.\n");
                    sprintf(kresult,"nan");
                    errorCode = EINVAL;
                    break;
                }
                out = in1 / in2;
                break;
            default:
                printk("SYSCALL_ERROR: sys_calc invalid operation %c.\n", operation);
                sprintf(kresult,"nan");
                errorCode = EINVAL;
                break;
        }
        // convert result to string
        if(!errorCode) sprintf(kresult,"%d", out);
    }
    else
    {
        switch(operation) {
            case '-':
                out = fpAddSub('-', in1, decPos1, in2, decPos2, &decPosOut);
                break;
            case '+':
                out = fpAddSub('+', in1, decPos1, in2, decPos2, &decPosOut);
                break;
            case '*':
                out = fpMult(in1, decPos1, in2, decPos2, &decPosOut);
                break;
            case '/':
                if(!in2)
                {
                    printk("SYSCALL_ERROR: sys_calc devided by zero.\n");
                    sprintf(kresult,"nan");
                    errorCode = EINVAL;
                    break;
                }
                out = fpDiv(in1, decPos1, in2, decPos2, &decPosOut);
                break;
            default:
                printk("SYSCALL_ERROR: sys_calc invalid operation %c.\n", operation);
                sprintf(kresult,"nan");
                errorCode = EINVAL;
                break;
        }
        if(!errorCode)
        {
            if(out == 0)
            {
                sprintf(kresult,"0");
            }
            else if(decPosOut >=0)
            {
                printDec(kresult, out, decPosOut);
            }
            else
            {
                for(i=0; i<(-decPosOut); ++i)
                {
                    moveTen *= 10;
                }
                sprintf(kresult,"%d", out*moveTen);
            }
        }
    }

    if( copy_to_user(result, kresult, strlen(kresult)*sizeof(char)) )
    {
        printk("SYSCALL_ERROR: failed in copy_to_user.\n");
        errorCode = EFAULT;
    }    
    return errorCode;
}



