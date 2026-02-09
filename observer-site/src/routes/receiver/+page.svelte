<script lang="ts">
	import type { MessageBody } from '$lib/types';
	import { onMount } from 'svelte';

	onMount(() => {
		main();
	});

	let ws: WebSocket;
	let pc: RTCPeerConnection;
	let id = $state('');

	function main() {
		ws = new WebSocket('ws://localhost:8080');
		ws.addEventListener('open', () => {
			console.log('connection established');
			console.log('sending ident message');
			const message = JSON.stringify({ type: 'ident', body: 'observer' });
			ws.send(message);
		});
		ws.addEventListener('close', () => {
			console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});

		pc = new RTCPeerConnection();
	}
	const handleClick = () => {};
	const messageHandler = async (event: MessageEvent) => {
		const data = JSON.parse(event.data);
		if (data.type === 'ident') {
			id = data.body.id;
		} else if (data.type === 'sd') {
			handleOffer(data.body);
		} else if (data.type === 'ice') {
			const iceCandidate = data.body.data;
			pc.addIceCandidate(iceCandidate);
		}
	};
	const handleOffer = async (offerMessage: MessageBody) => {
		const offer = offerMessage.data;
		const offerer = offerMessage.from;
		await pc.setRemoteDescription(offer as RTCSessionDescription);
		const answer = await pc.createAnswer(offer);
		pc.setLocalDescription(answer);
		const response = JSON.stringify({
			type: 'sd',
			body: { from: id, to: offerer, data: answer }
		});
		ws.send(response);
	};
</script>

<div>
	<textarea readonly></textarea>
	<button onclick={handleClick}>Click me</button>
</div>
