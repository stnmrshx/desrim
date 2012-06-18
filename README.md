Desrim - Fast robust and scalable heap manager, like a shrimp beibeh... like a shrimp...
README 1.0

--Intro meeeen

Desrim ini merupakan sebuah program yang berfungsi sebagai memory allocator 
atau heap manager alternatif dari malloc() dan standar memory allocator lainnya.
Tujuannya adalah sebagai memory allocator yang bisa menghandle aplikasi-aplikasi 
multithreaded, cepat, serta maknyos. Nama Desrim sendiri berasal dari kekaguman 
yang luar biasa dari saya kepada teman saya yaitu Komenk the Shrimp, seorang breakdancer 
dan multi talent personal. Dari kata The Shrimp lalu oleh rekan-rekan 1stlink diplesetkan 
menjadi DShrimp, hingga mengalami evolusi pengucapan menjadi DESRIM. Begitulah awal 
mulanya terciptanya, dan oleh masukan dari bapak SakitJiwa (sJ) maka disepakati untuk 
melekatkan nama DESRIM kepada memory allocator ini yang semoga saja bisa menjadi memory 
allocator mawadah, warohmah dan brikdenser layaknya Komenk the Shrimp.
Berangkat dari kesedihan luar biasa saya kepada memory allocator yang umum digunakan 
saat ini, yang sungguh elek sekali dalam menjaga vitalitas serta performance multithreadnya,
maka saya bikin sembari iseng-iseng aja memory allocator ini. Ya pokoke begitulah.
Ah iya hampir lupa, ini dibuat lisensinya BSD Lisence, sama dengan yang dipake FreeBSD aja, 
soalnya paling maknyos menurut saya

Desrim memory allocator idenya muncul setelah saya ngutak ngatik Hoard Memory Allocator, 
yang katanya sih Hoard itu multithreaded, tapiiipaaaak saya sedih bukan kepalang, lantaran itu Hoard 
dibikinnya pake C++ yang sumpah deh booo paling males saya liatnya karena pake cyin cyin gitu syntaxnya. 
Maka dari itu saya buat aja ini Desrim dari C, selain enteng, enteng jodoh, dan mudah dalam 
portabilitasnya serta kecil beud filenya.

Untuk saat ini karena Desrim Memory Allocator ini dibuat dari bahasa C dan maknyos sekali, maka dia 
bisa dipake untuk :

FreeBSD
Linux

Untuk Windows, MacOSX sama Solaris, ane belom nyoba, soalnyo ane gapunya huheuheuhue. Bisa buat 32bit en 64bit 
juga loooh. 

--Installation

Ahk enak bener kok ini installnya, cuma gini :

./configure
make
sudo make install

Udah deh ntar doi ada di /usr/local/lib disituuuuutuuuuuh


--Cara Pake

Gampang kok, compile aja applikasi ente pake nambahin flag "-ldsmalloc" sama "-ldesrim" aja, atau kalau 
mau dipake internal gitu tinggal ganti function malloc() jadi ds_malloc()



--Feedback

Kami ini anak baik-baik, jadi kami dengan tangan terbuka menerima semua bug report, komentar dan masukan. Kalo mau 
kirim semua itu silahkan kirim saja ke stnmrshx@gmail.com dan berhadiah piring cantik, gelas blink-blik dan payung 
unyuuuu loooh X)) nyahahahaha. Mwach mwach semuanya :*

Stn MrshX
stnmrshx@gmail.com
