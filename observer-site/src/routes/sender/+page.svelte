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
	let observerId = $state('');
	let iceBacklog: Array<RTCIceCandidate> = $state([]);

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

		ws = new WebSocket(SSURL);
		ws.addEventListener('open', () => {
			console.log('connection established');
		});
		ws.addEventListener('close', () => {
			console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});
	};

	const startVideo = () => {
		console.log('video');
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

	const handleNegotiation = async () => {
		console.log('track added');
		const offer = await pc.createOffer();

		pc.setLocalDescription(offer);
	};

	const handleIce = (event: RTCPeerConnectionIceEvent) => {
		console.log(event.candidate);
		if (event.candidate == null) {
			return;
		}
		if (observerId) {
			sendIceCandidate(event.candidate);
		} else {
			iceBacklog.push(event.candidate);
		}
	};

	const sendIceCandidate = (candidate: RTCIceCandidate) => {
		const message = JSON.stringify({
			type: 'ice',
			body: {
				from: id,
				to: observerId,
				data: JSON.stringify(candidate)
			}
		});
		ws.send(message);
	};

	const messageHandler = async (event: MessageEvent) => {
		const data = JSON.parse(event.data);
		if (data.type === 'ident') {
			id = data.body.id;
			console.log('sending ident message');
			const message = JSON.stringify({ type: 'ident', body: { to: '', from: id, data: 'drone' } });
			ws.send(message);
		} else if (data.type === 'sd') {
			const answerer = data.body.from;
			if (answerer != observerId) {
				console.log('invalid sender');
				return;
			}
			const answer = data.body.data;
			await pc.setRemoteDescription(answer);
		} else if (data.type === 'ice') {
			const iceCandidate = data.body.data;
			pc.addIceCandidate(iceCandidate);
		} else if (data.type === 'select') {
			observerId = data.body;
			iceBacklog.forEach((candidate: RTCIceCandidate) => {
				sendIceCandidate(candidate);
			});
		}
	};
</script>

<video autoplay></video>
<button onclick={startVideo} style="padding: 10px; background-color: beige;"
	>Start video stream</button
>
<pre
	style="max-width: 500px; white-space: normal; max-height: 200px; overflow: scroll;">{offerString}</pre>
