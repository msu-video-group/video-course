#include "metric.hpp"

long GetErrorSAD_16x16(const uint8_t* block1, const uint8_t* block2, const int stride)
{
    long sum = 0;
    short m[4] = {1,1,1,1};
    int b1 = (int)block1;
    int b2 = (int)block2;
    __asm
    {
        push edi
        push esi

        mov esi, b1//[esp + 8 + 4]    ; Current MB (Estimated MB)
        mov edi, b2//[esp + 8 + 12]    ; reference MB
        mov eax, stride//[esp + 8 + 20]    ; stride

        pxor mm7, mm7            ; mm7 = sum = 0
        pxor mm6, mm6            ; mm6 = 0

        movq mm1, [esi]            ; 8 pixels for current MB
        movq mm3, [esi+8]        ; another 8 pixels in the same row of current MB

        mov    ecx, 16
        //mov    ecx, 8

sad16_mmx_loop:
        movq mm0, [edi]            ; 8 pixels for reference MB
        movq mm2, [edi+8]        ; another 8 pixels in the same row of reference MB

        movq mm4, mm1
        movq mm5, mm3

        psubusb mm1, mm0
        psubusb mm3, mm2

        add    esi, eax
        add    edi, eax

        psubusb mm0, mm4
        psubusb mm2, mm5

        por mm0, mm1            ; mm0 = |cur - ref|
        por mm2, mm3            ; mm2 = |*(cur+8) - *(ref+8)|

        movq mm1,mm0
        movq mm3,mm2

        punpcklbw mm0,mm6
        punpckhbw mm1,mm6

        punpcklbw mm2,mm6
        punpckhbw mm3,mm6

        paddusw mm0,mm1
        paddusw mm2,mm3

        paddusw mm7,mm0        ; sum += mm01
        movq mm1, [esi]        ; 8 pixels for current MB

        paddusw mm7,mm2        ; sum += mm23
        movq mm3, [esi+8]    ; another 8 pixels in the same row of current MB

        ;// start next row's processing
        movq mm0, [edi]            ; 8 pixels for reference MB
        movq mm2, [edi+8]        ; another 8 pixels in the same row of reference MB

        movq mm4, mm1
        movq mm5, mm3

        psubusb mm1, mm0
        psubusb mm3, mm2

        add    esi, eax
        add    edi, eax

        psubusb mm0, mm4
        psubusb mm2, mm5

        por mm0, mm1            ; mm0 = |cur - ref|
        por mm2, mm3            ; mm2 = |*(cur+8) - *(ref+8)|

        movq mm1,mm0
        movq mm3,mm2

        punpcklbw mm0,mm6
        punpckhbw mm1,mm6

        punpcklbw mm2,mm6
        punpckhbw mm3,mm6

        paddusw mm0,mm1
        paddusw mm2,mm3

        paddusw mm7,mm0        ; sum += mm01
        movq mm1, [esi]        ; 8 pixels for current MB

        paddusw mm7,mm2        ; sum += mm23
        sub        ecx, 2        ; unlooped two rows processing

        movq mm3, [esi+8]    ; another 8 pixels in the same row of current MB

        jnz    sad16_mmx_loop

        pmaddwd mm7, m    ; merge sum
        pop     esi

        movq mm0, mm7
        pop     edi
        psrlq mm7, 32

        paddd mm0, mm7
        movd sum, mm0

        emms
    }
    return sum;
}

long GetErrorSAD_8x8(const uint8_t* block1, const uint8_t *block2, const int stride)
{
    long sum = 0;
    short m[4] = {1,1,1,1};
    int b1 = (int)block1;
    int b2 = (int)block2;
    __asm
    {
        push edi
        push esi

        mov esi, b1//[esp + 8 + 4]    ; Current MB (Estimated MB)
        mov edi, b2//[esp + 8 + 12]    ; reference MB
        mov eax, stride//[esp + 8 + 20]    ; stride

        pxor    mm6, mm6            ; mm6 = sum = 0

        pxor    mm7, mm7            ; mm7 = 0
        mov        ecx, 4

sad8_mmx_lp:

        movq mm0, [esi]    ; ref
        movq mm1, [edi]    ; cur

        add    esi, eax
        add    edi, eax

        movq mm2, [esi]    ; ref2
        movq mm3, [edi]    ; cur2

        movq mm4, mm0
        movq mm5, mm2

        psubusb mm0, mm1
        psubusb mm2, mm3

        psubusb mm1, mm4
        psubusb mm3, mm5

        por mm0, mm1            ; mm0 = |ref - cur|
        por mm2, mm3            ; mm2 = |*(ref+stride) - *(cur+stride)|

        movq mm1,mm0
        movq mm3,mm2

        punpcklbw mm0,mm7
        punpcklbw mm2,mm7

        punpckhbw mm1,mm7
        punpckhbw mm3,mm7

        paddusw mm0,mm1
        paddusw mm2,mm3

        paddusw mm6,mm0            ; sum += mm01
        add    esi, eax

        add    edi, eax
        paddusw mm6,mm2            ; sum += mm23

        dec    ecx
        jnz    sad8_mmx_lp

        pmaddwd mm6, m    ; merge sum
        pop     esi

        movq mm7, mm6
        pop     edi

        psrlq mm7, 32

        paddd mm6, mm7

        movd sum, mm6

        emms
    }
    return sum;
}
