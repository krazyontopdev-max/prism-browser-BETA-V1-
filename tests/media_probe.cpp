#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>
#include <QWebEngineProfile>
#include <QWebEngineDesktopMediaRequest>
#include <QWebEnginePermission>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QSignalSpy>
#include <memory>

class MediaProbe : public QObject {
    Q_OBJECT
private slots:
    void physicalDevices()
    {
        if (!qEnvironmentVariableIsSet("PRISM_DEVICE_PROBE")) QSKIP("Physical-device diagnostic requires explicit opt-in.");
        QWebEngineProfile profile; QWebEngineView view(&profile);
        view.resize(480, 320); view.show();
        connect(view.page(), &QWebEnginePage::permissionRequested, &view, [](QWebEnginePermission permission) {
            using Type = QWebEnginePermission::PermissionType;
            const auto type = permission.permissionType();
            if (type == Type::MediaAudioCapture || type == Type::MediaVideoCapture || type == Type::MediaAudioVideoCapture) permission.grant();
            else permission.deny();
        });
        QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
        view.setHtml(R"HTML(<title>Local device test</title><button style='display:block' onclick=start()>Start</button><video id=v autoplay muted playsinline width=160></video><script>
            window.result={state:'pending'};
            async function start(){
              let stream,context,timer,peak=0;
              const cleanup=()=>{clearInterval(timer);if(stream)stream.getTracks().forEach(t=>t.stop());if(context)context.close()};
              window.addEventListener('pagehide',cleanup,{once:true});
              try{
                window.result.stage='requesting devices';
                stream=await navigator.mediaDevices.getUserMedia({video:{width:{ideal:320},height:{ideal:240},frameRate:{ideal:10}},audio:true});
                v.srcObject=stream;
                context=new AudioContext();const analyser=context.createAnalyser();analyser.fftSize=256;
                context.createMediaStreamSource(stream).connect(analyser);window.result.stage='resuming audio';await context.resume();
                window.result.stage='capturing';
                const samples=new Uint8Array(analyser.fftSize),begin=Date.now();
                timer=setInterval(()=>{
                  analyser.getByteTimeDomainData(samples);for(const sample of samples)peak=Math.max(peak,Math.abs(sample-128));
                  const frames=v.getVideoPlaybackQuality().totalVideoFrames,audio=stream.getAudioTracks()[0];
                  if(Date.now()-begin>2000&&frames>=3&&audio&&audio.readyState==='live'){
                    window.result={state:'passed',videoFrames:frames,width:v.videoWidth,height:v.videoHeight,audioState:audio.readyState,audioPeak:peak};cleanup();
                  }else if(Date.now()-begin>10000){window.result={state:'timeout',videoFrames:frames};cleanup()}
                },100);
              }catch(e){window.result={state:'error',name:e.name,message:e.message};cleanup()}
            }</script>)HTML", QUrl("http://127.0.0.1/"));
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty() && !view.page()->isLoading(), 10000);
        QTRY_VERIFY(view.focusProxy());
        QTest::mouseClick(view.focusProxy(), Qt::LeftButton, Qt::NoModifier, QPoint(25, 18));
        QElapsedTimer deadline; deadline.start(); QJsonObject result;
        while (deadline.elapsed() < 20000) {
            auto sample = std::make_shared<QVariant>();
            view.page()->runJavaScript("window.result", [sample](const QVariant &value) { *sample = value; });
            QTRY_VERIFY_WITH_TIMEOUT(sample->isValid(), 3000);
            result = QJsonObject::fromVariantMap(sample->toMap());
            if (result.value("state").toString() != "pending") break;
            QTest::qWait(200);
        }
        QFile output(QCoreApplication::applicationDirPath() + "/device-results.json");
        QVERIFY(output.open(QIODevice::WriteOnly)); output.write(QJsonDocument(result).toJson());
        QCOMPARE(result.value("state").toString(), QString("passed"));
    }

    void screenSourceCancellation()
    {
        if (!qEnvironmentVariableIsSet("PRISM_SCREEN_PROBE")) QSKIP("Isolated desktop-source cancellation diagnostic only.");
        QWebEngineProfile profile; QWebEngineView view(&profile);
        view.resize(640, 400); view.show();
        view.settings()->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
        bool pickerCalled = false;
        connect(view.page(), &QWebEnginePage::desktopMediaRequested, &view, [&pickerCalled](const QWebEngineDesktopMediaRequest &request) {
            pickerCalled = true; request.cancel();
        });
        connect(view.page(), &QWebEnginePage::permissionRequested, &view, [](QWebEnginePermission permission) {
            if (permission.permissionType() == QWebEnginePermission::PermissionType::DesktopVideoCapture) permission.grant();
            else permission.deny();
        });
        QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
        view.setHtml("<title>Share probe</title><button onclick=\"navigator.mediaDevices.getDisplayMedia({video:true}).then(s=>{s.getTracks().forEach(t=>t.stop());document.title='Unexpected capture'}).catch(e=>{window.errorName=e.name;document.title='Capture denied'})\">Share</button>", QUrl("http://127.0.0.1/"));
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty() && !view.page()->isLoading(), 10000);
        QTRY_VERIFY(view.focusProxy());
        QTest::mouseClick(view.focusProxy(), Qt::LeftButton, Qt::NoModifier, QPoint(25, 18));
        QTRY_COMPARE_WITH_TIMEOUT(view.title(), QString("Capture denied"), 10000);
        QVERIFY(pickerCalled);
    }

    void localWebRtcLoopback()
    {
        QWebEngineProfile profile; QWebEngineView view(&profile);
        view.resize(640, 400); view.show();
        QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
        view.setHtml(R"HTML(<title>WebRTC loopback</title><canvas id=c width=160 height=90></canvas>
            <video id=remote autoplay muted playsinline></video><script>
            window.result={state:'pending'};
            (async()=>{
              const stream=c.captureStream(10), a=new RTCPeerConnection({iceServers:[]}), b=new RTCPeerConnection({iceServers:[]});
              let timer, paint, frame=0;
              const cleanup=()=>{clearInterval(timer);clearInterval(paint);stream.getTracks().forEach(t=>t.stop());a.close();b.close()};
              window.addEventListener('pagehide',cleanup,{once:true});
              try {
                a.onicecandidate=e=>{if(e.candidate)b.addIceCandidate(e.candidate).catch(()=>{})};
                b.onicecandidate=e=>{if(e.candidate)a.addIceCandidate(e.candidate).catch(()=>{})};
                b.ontrack=e=>{remote.srcObject=e.streams[0]};
                stream.getTracks().forEach(t=>a.addTrack(t,stream));
                await a.setLocalDescription(await a.createOffer());await b.setRemoteDescription(a.localDescription);
                await b.setLocalDescription(await b.createAnswer());await a.setRemoteDescription(b.localDescription);
                paint=setInterval(()=>{const ctx=c.getContext('2d');ctx.fillStyle=++frame%2?'red':'blue';ctx.fillRect(0,0,160,90)},100);
                const deadline=Date.now()+15000;
                timer=setInterval(async()=>{
                  for(const s of (await b.getStats()).values())if(s.type==='inbound-rtp'&&s.kind==='video'&&s.framesDecoded>2){
                    window.result={state:'passed',framesDecoded:s.framesDecoded,bytesReceived:s.bytesReceived,codec:'negotiated locally'};cleanup();return;
                  }
                  if(Date.now()>deadline){window.result={state:'timeout'};cleanup()}
                },200);
              }catch(e){window.result={state:'error',message:e.message};cleanup()}
            })();</script>)HTML", QUrl("file:///prism-loopback.html"));
        QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty() && !view.page()->isLoading(), 10000);
        QJsonObject result;
        QElapsedTimer deadline; deadline.start();
        while (deadline.elapsed() < 20000) {
            auto sample = std::make_shared<QVariant>();
            view.page()->runJavaScript("window.result", [sample](const QVariant &value) { *sample = value; });
            QTRY_VERIFY_WITH_TIMEOUT(sample->isValid(), 3000);
            result = QJsonObject::fromVariantMap(sample->toMap());
            if (result.value("state").toString() != "pending") break;
            QTest::qWait(200);
        }
        QFile output(QCoreApplication::applicationDirPath() + "/webrtc-results.json");
        QVERIFY(output.open(QIODevice::WriteOnly)); output.write(QJsonDocument(result).toJson());
        QCOMPARE(result.value("state").toString(), QString("passed"));
        QVERIFY(result.value("framesDecoded").toInt() > 2);
    }

    void playback()
    {
        QWebEngineProfile profile; QWebEngineView view(&profile); view.resize(640, 400); view.show();
        QJsonArray results;
        bool supportedPlayback = true;
        const bool requireProprietary = qEnvironmentVariableIsSet("PRISM_REQUIRE_PROPRIETARY_CODECS");
        for (const auto &name : {"vp9.webm", "av1.webm", "h264-aac.mp4", "aac.m4a", "pcm.wav"}) {
            const auto source = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/media/" + name);
            QVERIFY(QFile::exists(source.toLocalFile()));
            QSignalSpy loaded(&view, &QWebEngineView::loadFinished);
            view.setHtml("<title>Media probe</title><button style='display:block' onclick='v.play()'>Play</button><video id=v muted playsinline width=160 src='" + source.toString(QUrl::FullyEncoded) + "'></video>", source);
            QTRY_VERIFY(!loaded.isEmpty() && !view.page()->isLoading());
            QTRY_VERIFY(view.focusProxy());
            QTest::mouseClick(view.focusProxy(), Qt::LeftButton, Qt::NoModifier, QPoint(25, 18));
            QJsonObject result;
            for (int i = 0; i < 20; ++i) {
                QTest::qWait(100); auto sample = std::make_shared<QVariant>();
                view.page()->runJavaScript("({time:v.currentTime,frames:v.getVideoPlaybackQuality().totalVideoFrames,error:v.error?v.error.code:0})", [sample](const QVariant &value) { *sample = value; });
                QTRY_VERIFY(sample->isValid()); result = QJsonObject::fromVariantMap(sample->toMap());
                if (result.value("time").toDouble() > 0.1 || result.value("error").toInt()) break;
            }
            result["file"] = QString::fromUtf8(name); results.append(result);
            qInfo().noquote() << QJsonDocument(result).toJson(QJsonDocument::Compact);
            const auto fileName = QString::fromUtf8(name);
            if (requireProprietary || fileName == "vp9.webm" || fileName == "av1.webm" || fileName == "pcm.wav") {
                bool played = result.value("error").toInt() == 0 && result.value("time").toDouble() > 0.1;
                if (fileName.endsWith(".webm") || fileName.endsWith(".mp4")) played = played && result.value("frames").toInt() > 0;
                supportedPlayback = supportedPlayback && played;
            }
        }
        QFile output(QCoreApplication::applicationDirPath() + "/media-results.json"); QVERIFY(output.open(QIODevice::WriteOnly)); output.write(QJsonDocument(results).toJson());
        QVERIFY(supportedPlayback);
    }
};
int main(int argc, char **argv) { QStandardPaths::setTestModeEnabled(true); QApplication app(argc, argv); MediaProbe test; return QTest::qExec(&test, argc, argv); }
#include "media_probe.moc"
