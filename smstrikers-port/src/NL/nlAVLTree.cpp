#include "NL/nlAVLTree.h"

#include "types.h"

/**
 * Offset/Address/Size: 0x0 | 0x801CE120 | size: 0x490
 */
AVLTreeNode* AVLTreeUntemplated::RemoveAVLNode(AVLTreeNode** root, void* key, unsigned int height)
{
    AVLTreeNode** stack;
    signed char* pathInfo;
    unsigned int stackTop;
    unsigned int savedStackTop;
    int comp;
    AVLTreeNode* deleted;
    AVLTreeNode* curr;
    AVLTreeNode* prev;
    AVLTreeNode* min;
    AVLTreeNode* kid;
    AVLTreeNode* grandkid;

    if (*root == NULL)
    {
        return NULL;
    }

    stack = (AVLTreeNode**)__builtin_alloca((height + 1) * sizeof(AVLTreeNode*));
    pathInfo = (signed char*)__builtin_alloca(height + 1);

    stackTop = 0;
    curr = *root;

    while (true)
    {
        comp = CompareKey(key, curr);
        if (comp < 0)
        {
            pathInfo[stackTop] = -1;
            stack[stackTop] = curr;
            stackTop++;
            curr = curr->left;
        }
        else if (comp > 0)
        {
            pathInfo[stackTop] = 1;
            stack[stackTop] = curr;
            stackTop++;
            curr = curr->right;
        }
        else
        {
            break;
        }
        if (curr == NULL)
        {
            return NULL;
        }
    }

    if (curr->left == NULL)
    {
        if (stackTop == 0)
        {
            *root = curr->right;
        }
        else
        {
            prev = stack[stackTop - 1];
            if (curr == prev->left)
                prev->left = curr->right;
            else
                prev->right = curr->right;
        }
    }
    else if (curr->right == NULL)
    {
        if (stackTop == 0)
        {
            *root = curr->left;
        }
        else
        {
            prev = stack[stackTop - 1];
            if (curr == prev->left)
                prev->left = curr->left;
            else
                prev->right = curr->left;
        }
    }
    else
    {
        savedStackTop = stackTop;
        pathInfo[stackTop] = 1;
        stack[stackTop] = curr;
        stackTop++;
        min = curr->right;

        do
        {
            pathInfo[stackTop] = -1;
            stack[stackTop] = min;
            stackTop++;
            min = min->left;
        } while (min != NULL);

        stackTop--;
        min = stack[stackTop];

        if (savedStackTop == 0)
        {
            *root = min;
        }
        else
        {
            prev = stack[savedStackTop - 1];
            if (curr == prev->left)
                prev->left = min;
            else
                prev->right = min;
        }

        prev = stack[stackTop - 1];
        if (prev != curr)
        {
            prev->left = min->right;
            min->right = curr->right;
        }

        min->left = curr->left;
        min->heavy = curr->heavy;
        stack[savedStackTop] = min;
    }

    deleted = curr;

    while (stackTop > 0)
    {
        stackTop--;
        curr = stack[stackTop];

        if (curr->heavy == 0)
        {
            curr->heavy -= pathInfo[stackTop];
            break;
        }

        curr->heavy -= pathInfo[stackTop];
        if (curr->heavy == 0)
            continue;

        if (pathInfo[stackTop] == 1)
            kid = curr->left;
        else
            kid = curr->right;

        if (kid->heavy != pathInfo[stackTop])
        {
            if (pathInfo[stackTop] != 1)
            {
                curr->right = kid->left;
                kid->left = curr;
            }
            else
            {
                curr->left = kid->right;
                kid->right = curr;
            }

            if (stackTop != 0)
            {
                if (pathInfo[stackTop - 1] == 1)
                    stack[stackTop - 1]->right = kid;
                else
                    stack[stackTop - 1]->left = kid;
            }
            else
            {
                *root = kid;
            }

            if (kid->heavy == 0)
            {
                comp = -1;
                if (curr->heavy > 0)
                    comp = 1;

                curr->heavy = comp;
                kid->heavy = -curr->heavy;
                break;
            }

            kid->heavy = 0;
            curr->heavy = 0;
        }
        else
        {
            if (pathInfo[stackTop] != 1)
            {
                grandkid = kid->left;
                kid->left = grandkid->right;
                curr->right = grandkid->left;
                grandkid->right = kid;
                grandkid->left = curr;

                if (grandkid->heavy == 1)
                {
                    curr->heavy = -1;
                    kid->heavy = 0;
                }
                else if (grandkid->heavy == -1)
                {
                    curr->heavy = 0;
                    kid->heavy = 1;
                }
                else
                {
                    kid->heavy = 0;
                    curr->heavy = 0;
                }
            }
            else
            {
                grandkid = kid->right;
                kid->right = grandkid->left;
                curr->left = grandkid->right;
                grandkid->left = kid;
                grandkid->right = curr;

                if (grandkid->heavy == 1)
                {
                    curr->heavy = 0;
                    kid->heavy = -1;
                }
                else if (grandkid->heavy == -1)
                {
                    curr->heavy = 1;
                    kid->heavy = 0;
                }
                else
                {
                    curr->heavy = 0;
                    kid->heavy = 0;
                }
            }

            grandkid->heavy = 0;

            if (stackTop != 0)
            {
                if (pathInfo[stackTop - 1] == 1)
                    stack[stackTop - 1]->right = grandkid;
                else
                    stack[stackTop - 1]->left = grandkid;
            }
            else
            {
                *root = grandkid;
            }
        }
    }

    return deleted;
}

/**
 * Offset/Address/Size: 0x490 | 0x801CE5B0 | size: 0x398
 */
u32 AVLTreeUntemplated::AddAVLNode(AVLTreeNode** rootNode, void* key, void* value, AVLTreeNode** existingNode, unsigned int height)
{
    AVLTreeNode** root;
    AVLTreeNode** existing;
    AVLTreeUntemplated* self;
    void* searchKey;
    void* entryValue;
    AVLTreeNode* balance;
    AVLTreeNode* father;
    AVLTreeNode* next;
    AVLTreeNode* underBalance;
    AVLTreeNode* currSearch;
    AVLTreeNode* currBalance;
    AVLTreeNode* currAdjust;
    AVLTreeNode* newNode;
    unsigned int lessThanBalance;
    int comp;
    signed char* pathInfo;
    unsigned int stackTop;
    unsigned int balanceSpot;

    root = rootNode;
    existing = existingNode;
    self = this;
    searchKey = key;
    entryValue = value;

    *existing = NULL;

    next = *root;
    if (next == NULL)
    {
        *root = self->AllocateEntry(searchKey, entryValue);
        return 1;
    }

    balance = next;
    currSearch = next;
    father = NULL;

    pathInfo = (signed char*)__builtin_alloca(height + 1);

    stackTop = 0;
    balanceSpot = 0;

    while (true)
    {
        comp = self->CompareKey(searchKey, currSearch);
        pathInfo[stackTop] = comp;
        stackTop++;

        if (comp == 0)
        {
            *existing = currSearch;
            return 0;
        }

        if (comp > 0)
        {
            next = currSearch->right;
            if (next == NULL)
            {
                newNode = self->AllocateEntry(searchKey, entryValue);
                currSearch->right = newNode;
                break;
            }
        }
        else
        {
            next = currSearch->left;
            if (next == NULL)
            {
                newNode = self->AllocateEntry(searchKey, entryValue);
                currSearch->left = newNode;
                break;
            }
        }

        if (next->heavy != 0)
        {
            father = currSearch;
            balance = next;
            balanceSpot = stackTop;
        }

        currSearch = next;
    }

    comp = pathInfo[balanceSpot];
    lessThanBalance = ((unsigned int)comp) >> 31;

    if (lessThanBalance)
    {
        underBalance = balance->left;
    }
    else
    {
        underBalance = balance->right;
    }

    currAdjust = underBalance;
    while (currAdjust != newNode)
    {
        signed char path = pathInfo[++balanceSpot];

        if (path < 0)
        {
            currAdjust->heavy = -1;
            currAdjust = currAdjust->left;
        }
        else
        {
            currAdjust->heavy = 1;
            currAdjust = currAdjust->right;
        }
    }

    if (lessThanBalance)
    {
        if (balance->heavy == 0)
        {
            balance->heavy = -1;
            return 1;
        }

        if (balance->heavy < 0)
        {
            if (underBalance->heavy < 0)
            {
                balance->left = underBalance->right;
                underBalance->right = balance;
                balance->heavy = 0;
                underBalance->heavy = 0;
                currBalance = underBalance;
            }
            else
            {
                currBalance = underBalance->right;
                underBalance->right = currBalance->left;
                currBalance->left = underBalance;
                balance->left = currBalance->right;
                currBalance->right = balance;

                if (currBalance->heavy == 0)
                {
                    balance->heavy = 0;
                    underBalance->heavy = 0;
                }
                else if (currBalance->heavy < 0)
                {
                    balance->heavy = 1;
                    underBalance->heavy = 0;
                }
                else
                {
                    balance->heavy = 0;
                    underBalance->heavy = -1;
                }

                currBalance->heavy = 0;
            }
        }
        else
        {
            balance->heavy = 0;
            return 0;
        }
    }
    else
    {
        if (balance->heavy == 0)
        {
            balance->heavy = 1;
            return 1;
        }

        if (balance->heavy > 0)
        {
            if (underBalance->heavy > 0)
            {
                balance->right = underBalance->left;
                underBalance->left = balance;
                balance->heavy = 0;
                underBalance->heavy = 0;
                currBalance = underBalance;
            }
            else
            {
                currBalance = underBalance->left;
                underBalance->left = currBalance->right;
                currBalance->right = underBalance;
                balance->right = currBalance->left;
                currBalance->left = balance;

                if (currBalance->heavy == 0)
                {
                    balance->heavy = 0;
                    underBalance->heavy = 0;
                }
                else if (currBalance->heavy > 0)
                {
                    balance->heavy = -1;
                    underBalance->heavy = 0;
                }
                else
                {
                    balance->heavy = 0;
                    underBalance->heavy = 1;
                }

                currBalance->heavy = 0;
            }
        }
        else
        {
            balance->heavy = 0;
            return 0;
        }
    }

    if (father == NULL)
    {
        *root = currBalance;
    }
    else if (balance == father->right)
    {
        father->right = currBalance;
    }
    else
    {
        father->left = currBalance;
    }

    return 0;
}
