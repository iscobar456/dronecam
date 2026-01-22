<script lang="ts">
	import { onMount } from 'svelte';

	onMount(() => {
		main();
	});

	let offer = $state({});
	let offerString = $derived(JSON.stringify(offer));
	let pc: RTCPeerConnection;

	const main = () => {
		const config = {
			iceServers: [
				{ urls: 'stun:stun.barracuda.com:3478' },
				{ urls: 'stun:stun.actionvoip.com:3478' }
			]
		};
		pc = new RTCPeerConnection(config);
		pc.onnegotiationneeded = handleNegotiation;
		pc.onicecandidate = handleIce;
		navigator.mediaDevices.getUserMedia({ video: true }).then((stream) => {
			const videoElement = document.querySelector('video');
			if (videoElement) {
				videoElement.srcObject = stream;
			}
			stream.getTracks().forEach((track) => {
				console.log(track);
				pc.addTrack(track, stream);
			});
		});
	};

	const handleNegotiation = () => {
		console.log('track added');
		pc.createOffer().then((o) => (offer = o));
		pc.setLocalDescription(offer);
	};

	const handleIce = (event: RTCPeerConnectionIceEvent) => {
		console.log(event.candidate);
	};
</script>

<h2>Sending this video</h2>
<video autoplay></video>

<pre
	style="max-width: 500px; white-space: normal; max-height: 200px; overflow: scroll;">{offerString}</pre>
