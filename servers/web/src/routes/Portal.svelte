<script lang="ts">
	let { stream, disconnect } = $props();
	import ExitSvg from '$lib/assets/exit-svgrepo-com.svg';
</script>

<div
	style:display={stream == null ? 'none' : 'flex'}
	class="fixed inset-0 h-screen w-screen bg-gray-500 p-10"
>
	<video
		id="remoteVideo"
		class="w-full rounded-md bg-black"
		srcobject={stream}
		autoplay
		playsinline
		muted
	>
	</video>

	<button
		class="absolute top-0 right-0 m-1 h-8 w-8 cursor-pointer"
		onclick={() => {
			disconnect();
		}}><img src={ExitSvg} alt="Exit icon" /></button
	>
</div>
