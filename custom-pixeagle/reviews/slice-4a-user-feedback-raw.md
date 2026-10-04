# Slice 4a operator feedback — first desktop run

User report, 2026-09-27, retained before the follow-up fix:

> the text in setting fo rbackend sees white unreadable
>
> and when stoppe dwhatn to open again I see error hadnle this robsut as well
>
> `^C^C`
>
> `custom-pixeagle/validation/run-sih-desktop.sh --video test9`
>
> `OSError: [Errno 98] Address already in use`
>
> once ready to test again let me knwo I test

The report pointed to the private QGC/backend logs. A copy of the original
session logs was preserved under
`/home/alireza/.cache/pixeagle-qgc-baseline/slice-4-2026-09-26/sih-live-v1/logs/user-report-2026-09-27/`
before reproducing the launcher failure. This is direct user feedback; the
subsequent screenshots and automated checks are separate observations.

## Second desktop attempt — 2026-09-27

Verbatim user feedback (credentials and IDE context omitted):

> some problems..backend address was still empty .. and on my darak mode the place hodler text was nto visible. it shuld be in a way that is user left unchagned ituses default lcoal host and default prt.    but mos t imrpatnlty, when I select conenct it didnt owrk at atll . all screen remain balk. alll ocntorl disable. I opened dajdbaor.d it lads up it broght the camera . but teh cmear palyabck is nto real time. it just process the hwoel video in less than a second and keep repreatin.g I rmeber I ahd implmetned somethign (at least on gstreamer I remembr) that force teh system effeicnt best practice alwaay guratnee real time video placy back wether the system be so slow or so fast process. fix that as well and fix all problems.  nd ifnally as UI/UX idea. I think in the widget in colpased mode we onl have the clasic traceker and naem of active tracker and follwoer with their status (deisgn minimal clean operatior firenodly Ui/Ux dsign) .btu when click on options ,  we can then selec ttype of tracker or follwoer and mroe options if needed .. fix these adn tel mewhen ready to test agani .

## Third desktop attempt — 2026-09-27

> its not working and its wiered UI/UX and shows all unavailbel

The attached screenshot shows a signed-in but unverified Vehicle 1, no video,
and a bottom panel reporting unavailable target/following state. There is no
verification action on that screen. This description is an agent transcription
of visible screen content, not additional user feedback.

## Fourth desktop attempt — 2026-09-27

Verbatim user feedback (credentials and IDE context omitted):

> ok I did some test syou can check logs: I list my observatioan :
> i t aleast wrok. buyt the widget UI/UX is a catastropihoc. I epxalin rough what I ahv ein midne . you onsult with expert QGC and droen oepratior real world UI/UX deisgner adn rebuild . I want intially in comparct mode have teh option (just like in detailed view) classi or tracker. based on which is selcted, a verey celan text shows active tracker and active follow(easy intuuive depctale at a glacne) and their status with color or indicators being shows reported. once active trcaker then we can start following(etls have similalr QGC for holding a button to confirm and animatinos and .. jsut liek al lqgc actions) and when active we can stop following, for stop following we dont need that extra confirmaiton . and amek sur estate of the icons are robsut consient ans relaible and not stuck on somethign on stale dta or .. consider asl lapesects as expert then wehn user lcick on options the opened dialogue msut give user some new items to chagen active tracker if classic is slected ana naem of model is smart is selcted. current dialouge widget is too text based and too messyconfusing. adhere to QGC guidlines wher the operation dashbrod must have minima or none alwasy visble texts. even icons better use less texxts and more visual icons. or unnecessary setings liek tap to contorl checbox can remain in setting spage only. Istil in doube how we shld handle gimbal mode later with this . also consider do rthat . since it was a classic type tracke when we enable? but in this specifi cgimablmode we are using has it self both a smart mode and classic again inside it. feel free to chagne in pixeagle the implemtnio for futuer proof comptiablity with differnet gimabls systems and modules and camera and how we strucurred them.
> also maeksure the widget on screen is moveable with drag so user can move if it was obscuring somethign , sane with the status widget or pixeagle on top (or mayeb this status can be movd to the QGC top bar near other items like flight mode, VTOL etc toa void obscuring the view . once these solved let me knwo again to test. for test I might take of the droen agian. selct a tareget anda enable a follow tos ee how it works.

## Compact panel follow-up — 2026-09-27

> I liek this UI much more.. but on top of it for sitaion awareness and cliary putt the tiel somethign PIxEagle ro wahtever you recoememdn so user at a glacne know this widget is for PixEgale. and als I liek the test vide you creted for tracer test. maybe you can haev a new test video with known positno of the target and move it even along the screen at specifi times we know for testing behviaour of the tracker adn followe. teh otion dilaougue is also look nic ebut make sure we haev easy selct active modfel and active classisc tracker . and link to settings page ofQGC fo rPixEagel also be ther for easy access. and very clean minimal well placed and well aligned link to dahobrd of PixEAgle as well
