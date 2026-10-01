#include "UxSound.h"
#include "UxSoundM5.h"
#include "UxSoundPcm.h"
#include <algorithm>
#include <assert.h>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
using namespace ux::sound;

static std::vector<int16_t> render(Source& source, size_t block=256) {
    std::vector<int16_t> audio; int16_t buffer[1024];
    assert(block<=1024);
    unsigned limit=0;
    while(source.active()) {
        size_t n=source.render(buffer,block); assert(n && n<=block);
        audio.insert(audio.end(),buffer,buffer+n);
        for(size_t i=n;i<block;++i) assert(buffer[i]==0);
        assert(++limit<100000);
    }
    return audio;
}
static void u16(std::ofstream& f,uint16_t n) { f.put(n&255); f.put(n>>8); }
static void u32(std::ofstream& f,uint32_t n) { u16(f,n&65535);u16(f,n>>16); }
static void wav(const std::string& path,const std::vector<int16_t>& audio) {
    std::ofstream f(path,std::ios::binary);assert(f.good());
    f.write("RIFF",4);u32(f,36+audio.size()*2);f.write("WAVEfmt ",8);u32(f,16);
    u16(f,1);u16(f,1);u32(f,SampleRate);u32(f,SampleRate*2);u16(f,2);u16(f,16);
    f.write("data",4);u32(f,audio.size()*2);for(auto s:audio)u16(f,static_cast<uint16_t>(s));
}
struct Slot { const int16_t* ptr;size_t size;std::vector<int16_t> snapshot;size_t consumed; };
struct Speaker {
    bool begin(){running=true;++beginCalls;return true;}void end(){running=false;++endCalls;}
    bool isRunning(){return running;}bool isEnabled(){return true;}
    std::vector<Slot> queue;std::vector<size_t> submittedLengths;
    unsigned submissions=0,beginCalls=0,endCalls=0;bool failNext=false,running=true;
    size_t isPlaying(uint8_t)const{return queue.size();}
    bool playRaw(const int16_t* p,size_t n,uint32_t hz,bool stereo,int repeat,uint8_t channel,bool stop) {
        assert(queue.size()<2 && hz==SampleRate&&!stereo&&repeat==1&&channel==6&&!stop);
        if(failNext){failNext=false;return false;}
        for(auto& slot:queue)assert(slot.ptr!=p); // producer cannot reuse a live buffer
        queue.push_back({p,n,std::vector<int16_t>(p,p+n),0});submittedLengths.push_back(n);++submissions;return true;
    }
    bool consumeSamples(size_t samples) {
        while(samples) {
            if(queue.empty())return false;
            Slot& s=queue.front();assert(std::equal(s.snapshot.begin(),s.snapshot.end(),s.ptr));
            size_t take=std::min(samples,s.size-s.consumed);s.consumed+=take;samples-=take;
            if(s.consumed==s.size)queue.erase(queue.begin());
        }
        return true;
    }
    void consume() { for(auto& s:queue)assert(std::equal(s.snapshot.begin(),s.snapshot.end(),s.ptr));if(!queue.empty())queue.erase(queue.begin()); }
};
template<class T> void pump(M5Output<T>& output) {
    output.transportStep(); output.update(); output.transportStep();
}
int main(int argc,char**argv) {
    assert(std::fabs(midiFrequency(69)-440)<.001f);
    assert(scaleMidi(60,5)==72 && scaleMidi(60,2,Scale::Minor)==63);
    std::string directory=argc>1?argv[1]:"/tmp";
    std::vector<int16_t> reel;unsigned maxPeak=0,maxJump=0;
    for(unsigned cue=0;cue<static_cast<unsigned>(Cue::Count);++cue) {
        Synth s;assert(s.play(static_cast<Cue>(cue)));auto a=render(s,127);assert(a.front()==0&&a.back()==0);
        Synth other;assert(other.play(static_cast<Cue>(cue)));assert(a==render(other,512));
        unsigned peak=0,jump=0;for(size_t i=0;i<a.size();++i){peak=std::max(peak,static_cast<unsigned>(std::abs(a[i])));if(i)jump=std::max(jump,static_cast<unsigned>(std::abs(a[i]-a[i-1])));}
        assert(peak>100 && peak<=15000);maxPeak=std::max(maxPeak,peak);maxJump=std::max(maxJump,jump);
        std::cout<<"cue="<<cue<<" samples="<<a.size()<<" ms="<<a.size()/16<<" peak="<<peak<<" max_sample_delta="<<jump<<"\n";
        wav(directory+"/cue-"+std::to_string(cue)+".wav",a);
        reel.insert(reel.end(),a.begin(),a.end());reel.insert(reel.end(),4000,0);
    }
    wav(directory+"/ui-cues.wav",reel);
    Note n;n.midi=69;n.durationMs=100;n.gapMs=20;n.timbre=Timbre::Sine;
    Synth duration;assert(duration.play(n));assert(render(duration).size()==1920);
    int16_t b[512];Synth interrupt;n.durationMs=1000;assert(interrupt.play(n,Priority::Notification));interrupt.render(b,300);
    assert(!interrupt.play(Cue::Tap));assert(interrupt.play(Cue::Error));assert(interrupt.render(b,128)==128);assert(b[127]==0);
    assert(interrupt.active());assert(interrupt.render(b,1)==1&&b[0]==0);
    interrupt.cancel();assert(interrupt.render(b,512)==128);assert(!interrupt.active());
    Synth disabled;disabled.setEnabled(false);assert(!disabled.play(Cue::Tap));disabled.setEnabled(true);assert(disabled.play(n));disabled.render(b,200);disabled.setEnabled(false);assert(render(disabled).size()==128);
    Synth invalid;Note bad=n;bad.durationMs=0;assert(!invalid.play(bad));assert(!invalid.play(nullptr,1));assert(!invalid.play(&n,9));
    // Smooth sustained sine, release, and endpoint behavior at a known low pitch.
    Synth sine;sine.setVolume(255);n.midi=57;n.timbre=Timbre::Sine;assert(sine.play(n));auto smooth=render(sine);int delta=0;for(size_t i=1;i<smooth.size();++i)delta=std::max(delta,std::abs(smooth[i]-smooth[i-1]));assert(delta<1000);
    // 8-bit signed/unsigned equality, exact 8k->16k duration, clipping/headroom.
    std::vector<int8_t> signedPcm(800);std::vector<uint8_t> unsignedPcm(800);
    for(unsigned i=0;i<800;++i){signedPcm[i]=static_cast<int8_t>(110*std::sin(i*6.283185307*440/8000));unsignedPcm[i]=signedPcm[i]+128;}
    PcmClip clip;clip.data=signedPcm.data();clip.samples=signedPcm.size();clip.sampleRate=8000;clip.encoding=PcmEncoding::Signed8;
    PcmPlayer p;assert(p.play(clip));auto pcm=render(p,91);assert(pcm.size()==1600&&pcm.front()==0&&pcm.back()==0);
    clip.data=unsignedPcm.data();clip.encoding=PcmEncoding::Unsigned8;assert(p.play(clip));assert(pcm==render(p,512));wav(directory+"/pcm8-demo.wav",pcm);
    assert(p.play(clip,Priority::Alert));assert(!p.play(clip,Priority::Ambient));p.render(b,200);p.cancel();assert(render(p).size()==128);p.setEnabled(false);assert(!p.play(clip));
    // Each selectable timbre is distinct and bounded even at maximum gain/velocity.
    std::vector<std::vector<int16_t>> timbres;
    for(Timbre timbre: {Timbre::Sine,Timbre::Bell,Timbre::SoftSquare,Timbre::Pluck,Timbre::Chime,Timbre::Noise}) {
        Synth voice;voice.setVolume(255);Note note;note.timbre=timbre;note.velocity=255;
        assert(voice.play(note));auto signal=render(voice,73);
        assert(signal.front()==0&&signal.back()==0);
        for(auto sample:signal)assert(std::abs(sample)<=15000);
        for(const auto& previous:timbres)assert(previous!=signal);
        timbres.push_back(signal);
    }
    // All user volume steps are monotonic; an idle zero setting is truly silent.
    double previousSynth=-1,previousPcm=-1;
    for(uint8_t volume: {0,64,120,180,220,255}) {
        Synth level;level.setVolume(volume);assert(level.volume()==volume);
        assert(level.play(Cue::Confirm));auto signal=render(level);
        PcmPlayer pcmLevel;pcmLevel.setVolume(volume);assert(pcmLevel.volume()==volume);
        assert(pcmLevel.play(clip));auto pcmSignal=render(pcmLevel);
        double synthEnergy=0,pcmEnergy=0;
        for(auto sample:signal){assert(std::abs(sample)<=15000);synthEnergy+=double(sample)*sample;}
        for(auto sample:pcmSignal){assert(std::abs(sample)<=15000);pcmEnergy+=double(sample)*sample;}
        if(!volume)assert(synthEnergy==0&&pcmEnergy==0);
        assert(synthEnergy>previousSynth&&pcmEnergy>previousPcm);
        previousSynth=synthEnergy;previousPcm=pcmEnergy;
        level.setEnabled(false);pcmLevel.setEnabled(false);
        assert(level.volume()==volume&&pcmLevel.volume()==volume);
        assert(!level.enabled()&&!pcmLevel.enabled());
    }
    // Active gain changes retain a smooth envelope instead of a discontinuous step.
    Synth gain;gain.setVolume(255);Note low;low.midi=45;low.durationMs=500;low.timbre=Timbre::Sine;
    assert(gain.play(low));gain.render(b,512);int16_t last=b[511];gain.setVolume(0);
    assert(gain.render(b,512)==512);assert(std::abs(b[0]-last)<600);
    for(size_t i=1;i<512;++i)assert(std::abs(b[i]-b[i-1])<600);
    assert(std::abs(b[511])<5);
    Speaker speaker;Synth streamed;M5Output<Speaker> output;assert(output.begin(speaker,streamed));assert(!output.begin(speaker,streamed));assert(streamed.play(Cue::Success));assert(!output.setSource(p));
    for(unsigned i=0; i<100 && output.busy();++i){pump(output);assert(speaker.queue.size()<=2);speaker.consume();}
    assert(!output.busy()&&output.failures()==0&&speaker.submissions>=3);
    assert(output.setSource(p));assert(output.setSource(streamed));
    Speaker retrySpeaker;Synth retrySource;M5Output<Speaker> retryOutput;
    assert(retryOutput.begin(retrySpeaker,retrySource));assert(retrySource.play(Cue::Tap));retrySpeaker.failNext=true;
    pump(retryOutput);assert(retryOutput.failures()==1&&retryOutput.busy()&&retrySpeaker.queue.empty());
    pump(retryOutput);Synth reference;reference.play(Cue::Tap);assert(retrySpeaker.queue[0].snapshot==render(reference));
    // Muting preserves both Ready and Submitted tails, then fades the source.
    // Retirement can observe two occupied SDK slots becoming zero in one poll.
    for(bool alreadySubmitted: {false,true}) {
        Speaker muteSpeaker;Synth muted;M5Output<Speaker> muteOutput;
        Note sustained;sustained.durationMs=1000;
        assert(muteOutput.begin(muteSpeaker,muted));assert(muted.play(sustained));muteOutput.update();
        if(alreadySubmitted)muteOutput.transportStep();
        muted.setEnabled(false);assert(!muted.play(Cue::Tap));
        muteOutput.transportStep();assert(muteSpeaker.queue.size()==2);
        muteSpeaker.consume();muteSpeaker.consume();pump(muteOutput);
        assert(muteSpeaker.queue.size()==1&&muteSpeaker.queue[0].size==128);
        muteSpeaker.consume();muteOutput.transportStep();assert(!muteOutput.busy());
        muted.setEnabled(true);pump(muteOutput);assert(muteSpeaker.queue.empty());
        assert(muted.play(Cue::Tap));pump(muteOutput);
        Synth fresh;fresh.play(Cue::Tap);assert(muteSpeaker.queue[0].snapshot==render(fresh));
    }
    // Emulate M5's two borrowed-pointer slots at different playing-slot phases.
    // A 90/100 ms UI frame must not starve the sample-clock consumer.
    for(size_t phase: {size_t(0),size_t(1),size_t(1023),size_t(2047),size_t(2048),size_t(3072),size_t(4095)}) {
        for(size_t pauseMs: {size_t(90),size_t(100)}) {
            Speaker pausedSpeaker;Synth longSound;M5Output<Speaker> pausedOutput;
            n.durationMs=2000;n.gapMs=0;assert(longSound.play(n));assert(pausedOutput.begin(pausedSpeaker,longSound));
            pump(pausedOutput);assert(pausedSpeaker.consumeSamples(phase));pump(pausedOutput);
            assert(pausedSpeaker.consumeSamples(pauseMs*16));pump(pausedOutput);
            for(unsigned i=0;i<8;++i){assert(pausedSpeaker.consumeSamples(90*16));pump(pausedOutput);}
            assert(pausedOutput.failures()==0);
        }
    }
    // Real Synth cancellation keeps an 8 ms release active. Suspend must wait
    // for that release and both borrowed SDK buffers before ending hardware.
    Speaker sleepSpeaker;Synth sleepSynth;M5Output<Speaker> sleepOutput;
    Note sustained;sustained.durationMs=1000;sustained.gapMs=0;
    assert(sleepOutput.begin(sleepSpeaker,sleepSynth));assert(sleepSynth.play(sustained));
    sleepOutput.update();sleepOutput.transportStep();assert(sleepSpeaker.queue.size()==2);
    assert(sleepOutput.suspend());sleepOutput.update();assert(sleepSynth.active());
    for(unsigned i=0;i<20&&!sleepOutput.suspended();++i) {
        sleepSpeaker.consume();sleepOutput.transportStep();sleepOutput.update();sleepOutput.transportStep();
    }
    assert(sleepOutput.suspended()&&!sleepOutput.busy()&&sleepSpeaker.endCalls==1);
    assert(sleepSpeaker.submissions==3&&sleepSpeaker.submittedLengths.back()==128);
    // A cue accepted directly while suspended is canceled into the local sink;
    // resuming does not replay it or submit while hardware is off.
    const unsigned submissionsBefore=sleepSpeaker.submissions;
    assert(sleepSynth.play(Cue::Tap));sleepOutput.update();assert(!sleepSynth.active());
    assert(sleepOutput.resume());sleepOutput.transportStep();assert(sleepOutput.ready());
    sleepOutput.update();sleepOutput.transportStep();
    assert(sleepSpeaker.submissions==submissionsBefore&&sleepSpeaker.queue.empty());
    std::cout<<"all sound tests passed; max cue peak="<<maxPeak<<" max sample delta="<<maxJump<<" Synth bytes="<<sizeof(Synth)<<" M5Output bytes="<<sizeof(output)<<"\n";
}
