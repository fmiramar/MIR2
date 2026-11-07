(
Routine({
    try {
    var root=FreqToVolt.filenameSymbol.asString.dirname.dirname;
    var def=SynthDef(\espSmoke, { |freq=220|
        var x=SinOsc.ar(freq,0,4);
        var d=FreqToVolt.kr(x);
        var c=FreqToVolt.kr(x,mode:\circuit);
        var p=Periodicity2.kr(x);
        var e=EnvFollow2.kr(x);
        var t=TriggerDetector.kr(e);
        Out.ar(0,K2A.ar(d++c++p++[e]++t));
    });
    [16,64].do { |block|
        var options=ServerOptions.new, done=FlowVar.new, file, data, values;
        var hz=if(block==64,880,220);
        var path="/private/tmp" +/+ ("mir2-esp-"++block++".wav");
        options.numOutputBusChannels=15;options.numInputBusChannels=0;
        options.blockSize=block;
        options.ugenPluginsPath=[root+/+"build",Platform.resourceDir+/+"plugins"];
        Score([[0,[\d_recv,def.asBytes]],[0,[\s_new,\espSmoke,1000,0,0,\freq,hz]],[2,[\n_free,1000]]])
        .recordNRT(outputFilePath:path,sampleRate:48000,headerFormat:"wav",sampleFormat:"float",
            options:options,duration:2,action:{done.value=true});
        done.value;
        file=SoundFile.openRead(path);
        if(file.isNil) {Error("ESP render missing").throw};
        data=FloatArray.newClear(file.numFrames*15);file.readData(data);file.close;
        if(data.any {|v| v.isNaN or: {v<0} or: {v>1}}) {Error("ESP bounds failure").throw};
        values=Array.fill(15,{|i|data[(72000*15)+i]});
        if((values[0]-(hz/2000)).abs>0.02 or: {(values[4]-(hz/2000)).abs>0.02}
            or: {values[3]!=1} or: {values[7]!=1} or: {values[8]<0.98}
            or: {values[10]<0.98} or: {values[11]<0.35 or: {values[11]>0.68}}
            or: {values[13]!=1} or: {values[14]!=0}) {values.postln;Error("ESP output order/scaling failure").throw};
        if(data.clump(15).any{|f|f[12]==1}.not) {Error("ESP activation pulse missing").throw};
        File.delete(path);
        ("ESP_SMOKE_OK block="++block++" values="++values).postln;
    };
    0.exit;
    } { |error| error.reportError; 1.exit };
}).play(SystemClock);
)
