#include "mathview.h"

#include <ctype.h>
#include <string.h>

enum { LayoutLimit = 110, LayoutChars = 112 };
typedef enum { Text, Adjacent, Fraction, Radical, Raised, Parentheses } LayoutKind;
typedef struct {
    LayoutKind kind;
    int a,b;
    MathBox box;
    const char *text;
    int length;
    char single;
    bool small;
} Layout;
static Layout layouts[LayoutLimit];
static int count;
static bool overflow;

static int max(int a,int b) { return a>b ? a:b; }

static int item(Layout layout) {
    if (count == LayoutLimit-1) { overflow = true; return 0; }
    layouts[++count] = layout;
    return count;
}

static int leaf(const char *text,int length,char one,bool small) {
    return item((Layout){.kind=Text,.box={(small?4:7)*(one?1:length),small?4:7,small?4:7},
                          .text=text,.length=length,.single=one,.small=small});
}

static int join(LayoutKind kind,int a,int b) {
    if (!a || (kind != Radical && kind != Parentheses && !b)) { overflow = true; return 0; }
    MathBox left = layouts[a].box, right = layouts[b].box, box;
    switch (kind) {
        case Fraction:
            box = (MathBox){max(left.width,right.width)+4,left.above+left.below+3,right.above+right.below+3};
            break;
        case Radical: box = (MathBox){left.width+13,left.above+4,max(left.below,7)}; break;
        case Raised:
            box = (MathBox){left.width+right.width+1,max(left.above,left.above+right.above+right.below-4),left.below};
            break;
        case Parentheses: box = (MathBox){left.width+10,left.above,left.below}; break;
        default:
            box = (MathBox){left.width+right.width,max(left.above,right.above),max(left.below,right.below)};
            break;
    }
    return item((Layout){.kind=kind,.a=a,.b=b,.box=box});
}

static const char *closing(const char *first,const char *end) {
    int depth = 0;
    for (const char *p=first;p<end;p++) {
        if (*p=='(') depth++;
        if (*p==')' && --depth==0) return p;
    }
    return NULL;
}

static bool unary_mark(const char *start,const char *p,const char *end) {
    if (p==start) return true;
    const char *prev = p-1;
    while (prev>start && *prev==' ') prev--;
    if (*prev == 'e' || *prev == 'E')
        return prev>start && (isdigit((unsigned char)prev[-1]) || prev[-1]=='.') &&
               p+1<end && isdigit((unsigned char)p[1]);
    return strchr("+-*/^=(,",*prev)!=NULL;
}

static int parse_math(const char *start,const char *end,bool small,int depth);

static int fraction_term(const char *start,const char *end,bool small,int depth) {
    while (start<end && *start==' ') start++;
    while (start<end && end[-1]==' ') end--;
    if (start<end && *start=='(' && closing(start,end)==end-1) { start++; end--; }
    return parse_math(start,end,small,depth);
}

static int parse_math(const char *start,const char *end,bool small,int depth) {
    while (start<end && *start==' ') start++;
    while (start<end && end[-1]==' ') end--;
    if (overflow || depth>20 || start==end) return leaf(start,(int)(end-start),0,small);
    const char *level1 = NULL, *level2 = NULL, *power = NULL;
    int nesting = 0;
    for (const char *p=start;p<end;p++) {
        if (*p=='(') { nesting++; continue; }
        if (*p==')') { nesting--; continue; }
        if (nesting) continue;
        if ((*p=='+' || *p=='-') && !unary_mark(start,p,end)) {
            if (!level1 || (*level1!='=' && *level1!=MathApprox)) level1=p;
        } else if (*p=='=' || *p==MathApprox) level1=p;
        else if (*p=='*' || *p=='/') level2=p;
        else if (*p=='^' && !power && !level1 && !level2) power=p;
    }
    if (level1 || level2 || power) {
        const char *op = level1 ? level1 : level2 ? level2 : power;
        if (*op=='/') {
            int a = fraction_term(start,op,small,depth+1);
            int b = fraction_term(op+1,end,small,depth+1);
            return join(Fraction,a,b);
        }
        if (*op=='^') {
            int a = parse_math(start,op,small,depth+1);
            int b = fraction_term(op+1,end,true,depth+1);
            return join(Raised,a,b);
        }
        int a = parse_math(start,op,small,depth+1);
        int b = parse_math(op+1,end,small,depth+1);
        if (*op=='*' && ((end-op>5 && !strncmp(op+1,"sqrt(",5)) ||
                         (end-op==3 && !strncmp(op+1,"pi",2)))) return join(Adjacent,a,b);
        int symbol = leaf(NULL,0,*op=='*'?MathMultiply:*op,small);
        return join(Adjacent,join(Adjacent,a,symbol),b);
    }
    for (const char *p=start;p<end;p++) {
        if (*p!='(') continue;
        const char *close = closing(p,end);
        if (!close) break;
        bool root = p-start>=4 && !strncmp(p-4,"sqrt",4);
        const char *prefix_end = root ? p-4 : p;
        int child = parse_math(p+1,close,small,depth+1);
        int group = join(root?Radical:Parentheses,child,0);
        if (prefix_end>start) {
            int prefix = parse_math(start,prefix_end,small,depth+1);
            group = join(Adjacent,prefix,group);
        }
        if (close+1<end) {
            int suffix = parse_math(close+1,end,small,depth+1);
            group = join(Adjacent,group,suffix);
        }
        return group;
    }
    for (const char *p=start;p+1<end;p++) {
        if (p[0]!='p' || p[1]!='i') continue;
        int value = leaf(NULL,0,MathPi,small);
        if (p>start) value=join(Adjacent,parse_math(start,p,small,depth+1),value);
        if (p+2<end) value=join(Adjacent,value,parse_math(p+2,end,small,depth+1));
        return value;
    }
    return leaf(start,(int)(end-start),0,small);
}

static int prepare(const char *text,int width) {
    count=0;
    overflow=false;
    int root = parse_math(text,text+strlen(text),false,0);
    if (overflow || !root || layouts[root].box.width>width ||
        layouts[root].box.above+layouts[root].box.below>68) {
        count=0;
        int maxlen=width/7, length=(int)strlen(text);
        if (length>maxlen) length=maxlen;
        root=leaf(text,length,0,false);
    }
    return root;
}

MathBox measure_math(const char *text,int width) {
    return layouts[prepare(text,width)].box;
}

static void paint(int id,int x,int mid,const MathCanvas *c) {
    if (!id) return;
    const Layout *l=&layouts[id], *a=&layouts[l->a], *b=&layouts[l->b];
    switch (l->kind) {
        case Text: {
            char text[LayoutChars+1];
            if (l->single) { text[0]=l->single; text[1]=0; }
            else {
                int length=l->length<LayoutChars?l->length:LayoutChars;
                memcpy(text,l->text,(size_t)length);
                text[length]=0;
            }
            c->text(x,mid-l->box.above,text,c->ink,c->paper,l->small);
            break;
        }
        case Adjacent:
            paint(l->a,x,mid,c);
            paint(l->b,x+a->box.width,mid,c);
            break;
        case Fraction:
            c->rect(x,mid,l->box.width,1,c->ink);
            paint(l->a,x+(l->box.width-a->box.width)/2,mid-a->box.below-2,c);
            paint(l->b,x+(l->box.width-b->box.width)/2,mid+b->box.above+2,c);
            break;
        case Raised:
            paint(l->a,x,mid,c);
            paint(l->b,x+a->box.width+1,mid-a->box.above-b->box.below+4,c);
            break;
        case Radical: {
            int top=mid-a->box.above-2, bottom=mid+max(a->box.below-3,2);
            c->rect(x+1,mid+1,2,1,c->ink);
            for (int y=mid+1;y<=bottom;y++) c->rect(x+3+(y-mid-1)/2,y,1,1,c->ink);
            for (int y=top;y<=bottom;y++) c->rect(x+5+(bottom-y)*5/(bottom-top),y,1,1,c->ink);
            c->rect(x+10,top,l->box.width-10,1,c->ink);
            paint(l->a,x+12,mid,c);
            break;
        }
        case Parentheses: {
            int top=mid-l->box.above+1,bottom=mid+l->box.below-2;
            for (int y=top;y<=bottom;y++) {
                int d=y-top<bottom-y?y-top:bottom-y;
                int curve=d>=4?0:4-d;
                c->rect(x+curve,y,1,1,c->ink);
                c->rect(x+l->box.width-1-curve,y,1,1,c->ink);
            }
            paint(l->a,x+5,mid,c);
            break;
        }
    }
}

void paint_math(const char *text,int width,int x,int mid,const MathCanvas *canvas) {
    paint(prepare(text,width),x,mid,canvas);
}
