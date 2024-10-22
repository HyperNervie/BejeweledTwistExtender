#include "ccwtwist.h"

#include "../Hooks.h"

#include <Engine.h>
#include <Extender/util.h>
#include "mods.h"

//used to store the twist direction for frame perfect clicks since they call twistGems later
int btnClicked;
bool ccwValid;

namespace CCWTwistMod
{
    NAKEDDEF(boardClickFramePerfectOverride)
    {
        __asm
        {
            //original line
            mov[ebx + 15BCh], eax;

            //store which mouse button was clicked, for the frame perfect click
            mov eax, [ebp + 10h];
            mov btnClicked, eax;

            //return to original code
            push 0x6C73EF;
            ret;
        }
    }
    NAKEDDEF(updateBoardOverride) 
    {
        __asm
        {
            //original code
            mov eax, [ebx];
            push 1;

            //push the value we stored earlier, 0 for CW, 1 for CCW
            push btnClicked;

            //return to original code
            push 0x6D6863;
            ret;
        }
    }
    NAKEDDEF(keyPressedOverride)
    {
        __asm
        {
            //original code (unchanged), had to include these because of the 5 byte intercept
            //basically don't touch
            add eax, edi;
            push 1;

            //check if key we pressed is enter (ebp+8, D = enter, 20 = spacebar)
            cmp[ebp + 8], 0Dh;
            jz ccw;

            //do normal twist if we pressed spacebar (a6 = 0)
            push 0;
            jmp goback;
        
            //a6 = 1, same as if we clicked right click
        ccw:
            push 1;

            //return to original code
        goback:
            push 0x6CA5F9;
            ret;
        }
    }
    NAKEDDEF(boardClickOverride)
    {
        __asm
        {
            //check if a6 = 1, AKA check if we right-clicked
            mov eax, [ebp + 10h];
            xor al, isMouseInverted; //swap values if inverted
            cmp eax, 1;
            jz returnone;

            //we didn't, so set ccwtwist to 0
            mov byte ptr[ebx + 2015h], 0;
            jmp goback;

            //we did, so set ccwtwist to 1
        returnone:
            mov byte ptr[ebx + 2015h], 1;

        goback:

            // original code, restoring the compare because above changes interfere with next instruction
            cmp byte ptr[ebx + 25A4h], 0;

            // return to original code
            push 0x6C747A;
            ret;
        }
    }
    NAKEDDEF(TwistGemsOverride) 
    {
        __asm 
        {
            //altSound=1 - CW valid, CCW invalid; altSound=2 - the opposite
            mov eax, altSound;
            dec eax; //since altSound will always be 1 or 2, decrease it so we get a bool value for XOR
            xor eax, [ebp + 30h]; //flip around the twist direction corresponding to altSound value
            xor eax, 1; //flip the values again so people won't have to switch their own existing values in cfg
            xor al, isMouseInverted; //because mouseinvert doesn't directly invert the twist direction

            //at this point eax will be either 1 or 0 corresponding to the pitch we want
            //by multiplying eax with 5 and substracting 5 we get either 0 or -5 pitch
            imul eax, 5;
            sub eax, 5;
            push 0x6C7130;
            ret;
        }
    }
    NAKEDDEF(PrecheckMatchOverride)
    {
        __asm
        {
            //Change sub_6BB619 and call to check CCW matches
            mov byte ptr ds:0x6BB724, 0xC0; //or dword ptr [ebp-40],-01
            mov byte ptr ds:0x6BB728, 0xD4; //or dword ptr [ebp-2C],-01
            mov byte ptr ds:0x6BB732, 0xBC; //mov [ebp-44],eax
            mov byte ptr ds:0x6BB735, 0xB8; //mov [ebp-48],edi
            mov byte ptr ds:0x6BB738, 0xC4; //mov [ebp-3C],edi
            mov byte ptr ds:0x6BB73B, 0xC8; //mov [ebp-38],eax
            mov byte ptr ds:0x6BB73E, 0xCC; //mov [ebp-34],edi
            mov byte ptr ds:0x6BB741, 0xD0; //mov [ebp-30],edi
            push [ebp + 8];
            mov ecx, esi;
            push 1;
            mov eax, 0x6BB619;
            call eax;
            mov ccwValid, al;

            //Recover sub_6BB619
            mov byte ptr ds:0x6BB724, 0xCC; //or dword ptr [ebp-34],-01
            mov byte ptr ds:0x6BB728, 0xD0; //or dword ptr [ebp-30],-01
            mov byte ptr ds:0x6BB732, 0xB8; //mov [ebp-48],eax
            mov byte ptr ds:0x6BB735, 0xBC; //mov [ebp-44],edi
            mov byte ptr ds:0x6BB738, 0xC0; //mov [ebp-40],edi
            mov byte ptr ds:0x6BB73B, 0xC4; //mov [ebp-3C],eax
            mov byte ptr ds:0x6BB73E, 0xC8; //mov [ebp-38],edi
            mov byte ptr ds:0x6BB741, 0xD4; //mov [ebp-2C],edi

            push [ebp + 8];
            mov ecx, esi;
            push 1;
            push 0x6BD8A4;
            ret;
        }
    }
    NAKEDDEF(DoomCountdownOverride)
    {
        __asm
        {
            cmp byte ptr[esi + 2015h], 0;
            je clockwise;

            cmp ccwValid, 0;
            je invalidtwist;
            jmp validtwist;

        clockwise:
            cmp byte ptr[esi + 1C40h], 0;
            je invalidtwist;
            jmp validtwist;

        validtwist:
            push 0x6AA160;
            ret;

        invalidtwist:
            push 0x6AA16D;
            ret;
        }
    }
}

void initCCWTwistMod(CodeInjection::FuncInterceptor* hook)
{
    if (isCCWEnabled)
    {
        inject_byte(0x7D7BA6, 0x57); //increase FoV to differentiate screenshots
        inject_jmp(0x6C73E9, reinterpret_cast<void*>(CCWTwistMod::boardClickFramePerfectOverride));
        inject_nops(0x6C73EE, 1);
        inject_jmp(0x6D685E, reinterpret_cast<void*>(CCWTwistMod::updateBoardOverride));
        inject_jmp(0x6C7473, reinterpret_cast<void*>(CCWTwistMod::boardClickOverride));
        inject_nops(0x6C7478, 2);
        inject_jmp(0x6CA5F4, reinterpret_cast<void*>(CCWTwistMod::keyPressedOverride));
        inject_jmp(0x6BD89D, reinterpret_cast<void*>(CCWTwistMod::PrecheckMatchOverride));
        inject_jmp(0x6AA158, reinterpret_cast<void*>(CCWTwistMod::DoomCountdownOverride));
        inject_nops(0x6AA15D, 1);
        if (altSound)
            inject_jmp(0x6C7128, reinterpret_cast<void*>(CCWTwistMod::TwistGemsOverride));
        printf("Counter-Clockwise Twist Mod Initialized! %sSound set: %d\n", isMouseInverted ? "Mouse controls inverted. " : "", altSound);
    }
    else
        printf("Counter-Clockwise Twist Mod Initialized, but is disabled in config!\n");
}