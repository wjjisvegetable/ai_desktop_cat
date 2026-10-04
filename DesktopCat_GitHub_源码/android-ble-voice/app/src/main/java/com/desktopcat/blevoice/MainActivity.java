package com.desktopcat.blevoice;

import android.Manifest;
import android.app.Activity;
import android.bluetooth.*;
import android.bluetooth.le.*;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelUuid;
import android.provider.Settings;
import android.speech.RecognizerIntent;
import android.speech.tts.TextToSpeech;
import android.webkit.JavascriptInterface;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import org.json.JSONObject;
import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends Activity {
    private static final UUID SERVICE=UUID.fromString("42d60001-7568-4f24-9ca4-9d1a4d3e0001");
    private static final UUID COMMAND=UUID.fromString("42d60002-7568-4f24-9ca4-9d1a4d3e0001");
    private static final UUID STATUS=UUID.fromString("42d60003-7568-4f24-9ca4-9d1a4d3e0001");
    private static final UUID CCCD=UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private static final String ASSET="file:///android_asset/index.html";
    private final ExecutorService io=Executors.newFixedThreadPool(3);
    private WebView web;
    private TextToSpeech tts;
    private BluetoothAdapter adapter;
    private BluetoothLeScanner scanner;
    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic command;
    private volatile boolean bleReady=false,writePending=false;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        adapter=((BluetoothManager)getSystemService(BLUETOOTH_SERVICE)).getAdapter();
        getWindow().setStatusBarColor(Color.rgb(23,61,75));
        getWindow().setNavigationBarColor(Color.rgb(251,252,248));
        web=new WebView(this);
        WebSettings settings=web.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowUniversalAccessFromFileURLs(false);
        settings.setAllowFileAccessFromFileURLs(false);
        settings.setJavaScriptCanOpenWindowsAutomatically(false);
        web.setWebViewClient(new WebViewClient(){
            @Override public boolean shouldOverrideUrlLoading(WebView view,WebResourceRequest request){
                return !ASSET.equals(request.getUrl().toString());
            }
        });
        web.addJavascriptInterface(new Bridge(),"CatNative");
        setContentView(web);
        web.loadUrl(ASSET);
        tts=new TextToSpeech(this,code->{if(code==TextToSpeech.SUCCESS&&tts!=null)tts.setLanguage(Locale.SIMPLIFIED_CHINESE);});
    }
    private void js(String code){runOnUiThread(()->{if(web!=null)web.evaluateJavascript(code,null);});}
    private void state(String name,String detail){js("window.onBleState("+JSONObject.quote(name)+","+JSONObject.quote(detail)+")");}
    private void bleReply(String text){js("window.onBleReply("+JSONObject.quote(text)+")");}
    private void uiNotice(String text){js("window.onNativeNotice("+JSONObject.quote(text)+")");}
    private static URL lanUrl(String base,String path) throws Exception {
        URI uri=new URI(base);
        if(!"http".equals(uri.getScheme())||uri.getUserInfo()!=null||uri.getQuery()!=null||uri.getFragment()!=null||
           !(uri.getPath()==null||uri.getPath().isEmpty()||"/".equals(uri.getPath())))throw new IOException("只允许局域网 HTTP 地址");
        String host=uri.getHost();
        if(host==null||!host.matches("[0-9.]+"))throw new IOException("请填写局域网 IPv4 地址");
        String[] parts=host.split("\\.");
        if(parts.length!=4)throw new IOException("IPv4 地址不完整");
        int[] ip=new int[4];
        for(int i=0;i<4;i++){ip[i]=Integer.parseInt(parts[i]);if(ip[i]<0||ip[i]>255)throw new IOException("IPv4 地址范围错误");}
        if(!(ip[0]==10||(ip[0]==172&&ip[1]>=16&&ip[1]<=31)||(ip[0]==192&&ip[1]==168)))
            throw new IOException("只能访问私有局域网地址");
        if(uri.getPort()==0||uri.getPort()>65535)throw new IOException("端口无效");
        return new URL(uri.getScheme(),host,uri.getPort(),path);
    }
    private static String readText(InputStream stream) throws IOException {
        if(stream==null)return "无响应内容";
        try(InputStream input=stream;ByteArrayOutputStream out=new ByteArrayOutputStream()){
            byte[] buf=new byte[1024];int n;
            while((n=input.read(buf))!=-1){out.write(buf,0,n);if(out.size()>16384)throw new IOException("响应过长");}
            return out.toString("UTF-8");
        }
    }
    private static final class Result {final int code;final String body;Result(int c,String b){code=c;body=b;}}
    private static Result http(URL url,String method,String body,String contentType,String token,int timeout) throws IOException {
        HttpURLConnection conn=(HttpURLConnection)url.openConnection();
        try{
            conn.setInstanceFollowRedirects(false);
            conn.setRequestMethod(method);conn.setConnectTimeout(8000);conn.setReadTimeout(timeout);
            conn.setUseCaches(false);conn.setRequestProperty("Cache-Control","no-store");
            if(token!=null)conn.setRequestProperty("Authorization","Bearer "+token);
            if("POST".equals(method)){
                conn.setDoOutput(true);conn.setRequestProperty("Content-Type",contentType);
                try(OutputStream output=conn.getOutputStream()){output.write(body.getBytes(StandardCharsets.UTF_8));}
            }
            int code=conn.getResponseCode();
            return new Result(code,readText(code>=400?conn.getErrorStream():conn.getInputStream()));
        }finally{conn.disconnect();}
    }
    private final class Bridge {
        @JavascriptInterface public void request(String id,String base,String path,String method,String body){
            io.execute(()->{
                int code=0;String result;
                try{
                    if(!Arrays.asList("/api/status","/api/action","/api/mood").contains(path)||
                       !("GET".equals(method)||"POST".equals(method))||body.length()>1000)throw new IOException("接口请求无效");
                    Result response=http(lanUrl(base,path),method,body,"application/x-www-form-urlencoded; charset=utf-8",null,7000);
                    code=response.code;result=response.body;
                }catch(Exception e){result=e.getMessage();}
                js("window.onHttpReply("+JSONObject.quote(id)+","+code+","+JSONObject.quote(result)+")");
            });
        }
        @JavascriptInterface public void askCloud(String id,String base,String token,String body){
            io.execute(()->{
                int code=0;String result;
                try{
                    if(token.length()<16||token.length()>256||body.length()>16000)throw new IOException("代理参数无效");
                    Result response=http(lanUrl(base,"/ask"),"POST",body,"application/json; charset=utf-8",token,35000);
                    code=response.code;result=response.body;
                }catch(Exception e){result=e.getMessage();}
                js("window.onCloudReply("+JSONObject.quote(id)+","+code+","+JSONObject.quote(result)+")");
            });
        }
        @JavascriptInterface public void wifiSettings(){runOnUiThread(()->startActivity(new Intent(Settings.ACTION_WIFI_SETTINGS)));}
        @JavascriptInterface public void calibration(String base){runOnUiThread(()->{
            try{URL url=lanUrl(base,"/");startActivity(new Intent(Intent.ACTION_VIEW,Uri.parse(url.toString())));}
            catch(Exception e){uiNotice("校准地址错误："+e.getMessage());}
        });}
        @JavascriptInterface public void recognizeSpeech(){runOnUiThread(()->{
            Intent intent=new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
            intent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL,RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
            intent.putExtra(RecognizerIntent.EXTRA_LANGUAGE,"zh-CN");
            try{startActivityForResult(intent,2);}catch(Exception e){uiNotice("手机没有可用的语音识别服务");}
        });}
        @JavascriptInterface public void speak(String text){runOnUiThread(()->{if(tts!=null&&text.length()<=1000)tts.speak(text,TextToSpeech.QUEUE_FLUSH,null,"cat-reply");});}
        @JavascriptInterface public void bleConnect(){runOnUiThread(()->startBleScan());}
        @JavascriptInterface public void bleDisconnect(){runOnUiThread(()->disconnectBle());}
        @JavascriptInterface public void bleWrite(String payload){runOnUiThread(()->writeBle(payload));}
    }
    private boolean blePermissions(){
        String[] names=Build.VERSION.SDK_INT>=31?
            new String[]{Manifest.permission.BLUETOOTH_SCAN,Manifest.permission.BLUETOOTH_CONNECT}:
            new String[]{Manifest.permission.ACCESS_FINE_LOCATION};
        ArrayList<String> missing=new ArrayList<>();
        for(String name:names)if(checkSelfPermission(name)!=PackageManager.PERMISSION_GRANTED)missing.add(name);
        if(!missing.isEmpty()){requestPermissions(missing.toArray(new String[0]),1);return false;}
        return true;
    }
    @Override public void onRequestPermissionsResult(int request,String[] names,int[] grants){
        super.onRequestPermissionsResult(request,names,grants);
        if(request!=1)return;
        boolean allowed=grants.length>0;
        for(int grant:grants)allowed&=grant==PackageManager.PERMISSION_GRANTED;
        if(allowed)startBleScan();else state("error","需要附近设备权限才能连接 BLE");
    }
    private void startBleScan(){
        if(!blePermissions())return;
        if(adapter==null||!adapter.isEnabled()){state("error","请先开启手机蓝牙");return;}
        if(scanner!=null)scanner.stopScan(scanCallback);
        scanner=adapter.getBluetoothLeScanner();
        ScanFilter filter=new ScanFilter.Builder().setServiceUuid(new ParcelUuid(SERVICE)).build();
        scanner.startScan(Collections.singletonList(filter),new ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(),scanCallback);
        state("scanning","正在扫描 DesktopCat-S3…");
        web.postDelayed(()->{if(scanner!=null){scanner.stopScan(scanCallback);scanner=null;state("error","10 秒内未发现 S3，请确认已烧录新固件并上电");}},10000);
    }
    private void disconnectBle(){
        if(scanner!=null){scanner.stopScan(scanCallback);scanner=null;}
        if(gatt!=null){gatt.disconnect();gatt.close();gatt=null;}
        command=null;bleReady=false;writePending=false;state("idle","BLE 已断开");
    }
    private final ScanCallback scanCallback=new ScanCallback(){
        @Override public void onScanResult(int callbackType,ScanResult result){
            if(scanner!=null){scanner.stopScan(this);scanner=null;}
            state("connecting","发现设备，正在连接…");
            gatt=result.getDevice().connectGatt(MainActivity.this,false,gattCallback,BluetoothDevice.TRANSPORT_LE);
        }
        @Override public void onScanFailed(int code){state("error","BLE 扫描失败："+code);}
    };
    private final BluetoothGattCallback gattCallback=new BluetoothGattCallback(){
        @Override public void onConnectionStateChange(BluetoothGatt connection,int code,int newState){
            if(code==BluetoothGatt.GATT_SUCCESS&&newState==BluetoothProfile.STATE_CONNECTED){state("connecting","已连接，正在发现服务…");connection.discoverServices();}
            else{bleReady=false;writePending=false;command=null;state("error","BLE 已断开或连接失败："+code);connection.close();}
        }
        @Override public void onServicesDiscovered(BluetoothGatt connection,int code){
            BluetoothGattService service=code==BluetoothGatt.GATT_SUCCESS?connection.getService(SERVICE):null;
            if(service==null){state("error","未找到 DesktopCat GATT 服务");return;}
            command=service.getCharacteristic(COMMAND);
            BluetoothGattCharacteristic report=service.getCharacteristic(STATUS);
            if(command==null||report==null){state("error","BLE 服务特征不完整");return;}
            connection.setCharacteristicNotification(report,true);
            BluetoothGattDescriptor cccd=report.getDescriptor(CCCD);
            if(cccd==null){state("error","BLE 通知描述符缺失");return;}
            if(Build.VERSION.SDK_INT>=33)connection.writeDescriptor(cccd,BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            else{cccd.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);connection.writeDescriptor(cccd);}
        }
        @Override public void onDescriptorWrite(BluetoothGatt connection,BluetoothGattDescriptor item,int code){
            bleReady=code==BluetoothGatt.GATT_SUCCESS;
            state(bleReady?"ready":"error",bleReady?"BLE 已就绪；请先通过 Wi-Fi 校准并启动舵机":"BLE 回执订阅失败");
        }
        @Override public void onCharacteristicWrite(BluetoothGatt connection,BluetoothGattCharacteristic item,int code){
            writePending=false;if(code!=BluetoothGatt.GATT_SUCCESS)state("error","BLE 写入失败："+code);
        }
        @Override public void onCharacteristicChanged(BluetoothGatt connection,BluetoothGattCharacteristic item,byte[] value){
            bleReply(new String(value,StandardCharsets.UTF_8));
        }
        @SuppressWarnings("deprecation")
        @Override public void onCharacteristicChanged(BluetoothGatt connection,BluetoothGattCharacteristic item){
            bleReply(new String(item.getValue(),StandardCharsets.UTF_8));
        }
    };
    private void writeBle(String payload){
        if(!bleReady||gatt==null||command==null){state("error","BLE 尚未连接");return;}
        byte[] data=payload.getBytes(StandardCharsets.UTF_8);
        if(data.length>80||data.length<16||payload.indexOf('\0')>=0){state("error","BLE 指令长度错误");return;}
        if(writePending){state("error","上一条 BLE 指令尚未完成");return;}
        writePending=true;
        if(Build.VERSION.SDK_INT>=33){
            if(gatt.writeCharacteristic(command,data,BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)!=BluetoothStatusCodes.SUCCESS){writePending=false;state("error","BLE 写入未启动");}
        }else{command.setValue(data);if(!gatt.writeCharacteristic(command)){writePending=false;state("error","BLE 写入未启动");}}
    }
    @Override protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(request==2&&result==RESULT_OK&&data!=null){
            ArrayList<String> words=data.getStringArrayListExtra(RecognizerIntent.EXTRA_RESULTS);
            if(words!=null&&!words.isEmpty())js("window.onSpeechText("+JSONObject.quote(words.get(0))+")");
        }
    }
    @Override protected void onDestroy(){
        disconnectBle();io.shutdownNow();
        if(tts!=null)tts.shutdown();
        if(web!=null){web.destroy();web=null;}
        super.onDestroy();
    }
}
