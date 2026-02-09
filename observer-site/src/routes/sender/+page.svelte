<script lang="ts">
	import { onMount } from 'svelte';

	onMount(() => {
		main();
	});

	const SSURL = 'ws://localhost:8080';
	let offer = $state({});
	let offerString = $derived(JSON.stringify(offer));
	let pc: RTCPeerConnection;
	let ws: WebSocket;
	let id = $state('');

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

		ws = new WebSocket(SSURL);
		ws.addEventListener('open', () => {
			console.log('connection established');
			console.log('sending ident message');
			const message = JSON.stringify({ type: 'ident', body: 'drone' });
			ws.send(message);
		});
		ws.addEventListener('close', () => {
			console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});
	};

	const handleNegotiation = async () => {
		console.log('track added');
		const offer = await pc.createOffer();

		pc.setLocalDescription(offer);
	};

	const handleIce = (event: RTCPeerConnectionIceEvent) => {
		console.log(event.candidate);
	};

	const messageHandler = async (event: MessageEvent) => {
		const data = JSON.parse(event.data);
		if (data.type === 'ident') {
			id = data.body.id;
		} else if (data.type === 'sd') {
			const answer = data.body.data;
			const answerer = data.body.from;
			await pc.setRemoteDescription(answer);
		} else if (data.type === 'ice') {
			// const sender = data.body.from;
			const iceCandidate = data.body.data;
			pc.addIceCandidate(iceCandidate);
		}
	};
</script>

<h2>Sending this video</h2>
<video autoplay></video>

<pre
	style="max-width: 500px; white-space: normal; max-height: 200px; overflow: scroll;">{offerString}</pre>
