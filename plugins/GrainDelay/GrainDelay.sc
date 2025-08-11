GrainDelay : UGen {
	*ar { |input = 0, triggerRate = 10, overlap = 1, 
		delayTime = 0.2, grainRate = 1.0, mix = 0.5, 
		feedback = 0.3, damping = 0.7, freeze = 0, reset = 0|
		
		if(triggerRate.rate != 'audio') { triggerRate = K2A.ar(triggerRate) };
		if(overlap.rate != 'audio') { overlap = K2A.ar(overlap) };
		if(delayTime.rate != 'audio') { delayTime = K2A.ar(delayTime) };
		if(grainRate.rate != 'audio') { grainRate = K2A.ar(grainRate) };
		if(mix.rate != 'audio') { mix = K2A.ar(mix) };
		if(feedback.rate != 'audio') { feedback = K2A.ar(feedback) };
		if(damping.rate != 'audio') { damping = K2A.ar(damping) };
		if(freeze.rate != 'audio') { freeze = K2A.ar(freeze) };
		if(reset.rate != 'audio') { reset = K2A.ar(reset) };
		
		^this.multiNew('audio', input, triggerRate, overlap, 
			delayTime, grainRate, mix, feedback, damping, freeze, reset)
	}
}