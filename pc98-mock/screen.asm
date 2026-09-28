; SCREEN.COM — PC-98 版の画面の構造を実際に表示する試作 (QuuBee で確認済)。
;   テキスト画面を 1 行 24 ラスタ・字の上に 8 ラスタの帯 にし (GDC CSRFORM LR=17h + CRTC PL=18h/BL=0Fh/CL=10h)、
;   グラフィック画面 (16 色) に背景とふりがな (パレット 8 = 灰) を置く。キーを押すと 25 行に戻して終わる。
; データは gen_screen.py が out/ に作る。組み立ては build.sh (nasm -I out/ -f bin -o out/SCREEN.COM screen.asm)
        org 100h
        cpu 186

        mov ah, 42h             ; グラフィック 640x400
        mov ch, 0C0h
        int 18h
        mov ah, 40h             ; グラフィック表示開始
        int 18h
        mov ah, 12h             ; カーソルを消す
        int 18h

        mov al, 1               ; 16 色 (アナログ) モード
        out 6Ah, al
        mov si, pal             ; パレット 16 色 (G,R,B)
        xor bl, bl
.pal:   mov al, bl
        out 0A8h, al
        lodsb
        out 0AAh, al
        lodsb
        out 0ACh, al
        lodsb
        out 0AEh, al
        inc bl
        cmp bl, 16
        jb .pal

        cld
        mov si, planes          ; 4 プレーンを RLE から展開 (B・R・G・I の順)
        mov bx, segs
.plane: mov ax, [bx]
        mov es, ax
        xor di, di
        mov dx, 32000           ; 1 プレーンのバイト数
.run:   lodsb                   ; 個数
        xor ch, ch
        mov cl, al
        lodsb                   ; 値
        sub dx, cx
        rep stosb
        test dx, dx
        jnz .run
        add bx, 2
        cmp bx, segs + 8
        jb .plane

        mov ax, 0A000h          ; テキスト VRAM の文字
        mov es, ax
        xor di, di
        mov si, tcodes
        mov cx, 80*17
        rep movsw
        mov ax, 0A200h          ; 属性
        mov es, ax
        xor di, di
        mov si, tattrs
        mov cx, 80*17
        rep movsw

        ; CRTC: 1 行 24 ラスタ・字の上に 8 ラスタ (ruby24.asm と同じ)
        mov al, 18h
        out 70h, al
        mov al, 0Fh
        out 72h, al
        mov al, 10h
        out 74h, al
        xor al, al
        out 76h, al
.fifo:  in al, 60h
        test al, 4
        jz .fifo
        mov al, 4Bh             ; GDC CSRFORM: LR = 17h
        out 62h, al
        mov al, 17h
        out 60h, al
        xor al, al
        out 60h, al
        mov al, 0BBh
        out 60h, al

        xor ah, ah              ; キーを待つ
        int 18h

        mov ah, 0Ah             ; 25 行に戻す
        xor al, al
        int 18h
        mov ah, 41h
        int 18h
        mov ah, 16h
        mov dx, 0E120h
        int 18h
        mov ah, 11h
        int 18h
        mov ax, 4C00h
        int 21h

segs:   dw 0A800h, 0B000h, 0B800h, 0E000h
pal:    incbin "pal.bin"
tcodes: incbin "text.bin"
tattrs: incbin "attr.bin"
planes: incbin "planes.rle"
