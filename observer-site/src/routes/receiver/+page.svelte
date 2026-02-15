<script lang="ts">
	import type { MessageBody } from '$lib/types';
	import { onMount } from 'svelte';

	onMount(() => {
		main();
	});

	let ws: WebSocket;
	let pc: RTCPeerConnection;
	let id = $state('');
	let peerId = $state('');

	function main() {
		ws = new WebSocket('ws://localhost:8080');
		ws.addEventListener('open', () => {
			console.log('connection established');
		});
		ws.addEventListener('close', () => {
			console.log('connection closed');
		});
		ws.addEventListener('message', messageHandler);
		ws.addEventListener('error', () => {});

		pc = new RTCPeerConnection();
	}
	const handleClick = () => {
		const message = {
			type: 'list',
			body: {
				to: '',
				from: id,
				data: ''
			}
		};
		ws.send(JSON.stringify(message));
	};
	const messageHandler = async (event: MessageEvent) => {
		const message = JSON.parse(event.data);
		if (message.type === 'ident') {
			id = message.body.id;
			console.log('sending ident message');
			const response = JSON.stringify({
				type: 'ident',
				body: { to: '', from: id, data: 'observer' }
			});
			ws.send(response);
		} else if (message.type === 'sd') {
			handleOffer(message.body);
		} else if (message.type === 'ice') {
			const iceCandidate = message.body.data;
			pc.addIceCandidate(iceCandidate);
		} else if (message.type === 'list') {
			console.log(message.body);
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
			body: { from: id, to: offerer, data: JSON.stringify(answer) }
		});
		ws.send(response);
	};
</script>

<div>
	<textarea readonly></textarea>
	<button onclick={handleClick}>Click me</button>
	<input type="text" bind:value={peerId} />
	<p>{peerId}</p>
</div>
