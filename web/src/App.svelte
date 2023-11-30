<script lang="ts">
  import { onMount } from "svelte";
  import type { MouseEventHandler } from "svelte/elements";

  let canvas: HTMLCanvasElement;
  let ctx: CanvasRenderingContext2D;
  let start: { x: number; y: number } | null = null;

  let config: {
    rectangles: { x: number; y: number; width: number; height: number }[];
  } = {
    rectangles: [],
  };
  let buffer: ArrayBuffer;
  let log: string = "";

  onMount(() => {
    ctx = canvas.getContext("2d")!;
    fetchConfiguation().then(() => fetchImg());
  });

  const grayScaleToRGBAArray = (grayScale: Uint8ClampedArray) => {
    const rgb = new Uint8ClampedArray(grayScale.length * 4);
    for (let i = 0; i < grayScale.length; i++) {
      rgb[i * 4] = grayScale[i];
      rgb[i * 4 + 1] = grayScale[i];
      rgb[i * 4 + 2] = grayScale[i];
      rgb[i * 4 + 3] = 255;
    }
    return rgb;
  };

  const fetchConfiguation = () =>
    fetch("http://10.0.0.146/config.json")
      .then((res) => res.json())
      .then((c) => (config = c));

  const fetchImg = () =>
    fetch("http://10.0.0.146/image")
      .then((res) => res.arrayBuffer())
      .then((b) => {
        // Save buffer for later
        buffer = b;
        // Buffer contains raw grayscale image data, let's draw it
        drawBuffer();
        // Draw rectangles
        drawRectangles();
        // Fetch log
        fetchLog();
      });

  const fetchLog = () => {
    fetch("http://10.0.0.146/log.txt")
      .then((res) => res.text())
      .then((t) => {
        log = t;
      });
  };

  const drawBuffer = () => {
    const img = new ImageData(
      grayScaleToRGBAArray(new Uint8ClampedArray(buffer)),
      480,
      320
    );
    ctx.putImageData(img, 0, 0);
  };

  const drawRectangles = () => {
    config.rectangles.forEach((r) => {
      ctx.beginPath();
      ctx.rect(r.x, r.y, r.width, r.height);
      ctx.strokeStyle = "red";
      ctx.stroke();
    });
  };

  const onMouseDown: MouseEventHandler<HTMLCanvasElement> = (e) => {
    start = { x: e.offsetX, y: e.offsetY };
  };

  const onMouseUp: MouseEventHandler<HTMLCanvasElement> = (e) => {
    const end = { x: e.offsetX, y: e.offsetY };

    if (start) {
      // Add rectangle to list
      config = {
        ...config,
        rectangles: [
          ...config.rectangles,
          {
            x: start.x,
            y: start.y,
            width: end.x - start.x,
            height: end.y - start.y,
          },
        ],
      };
      // Draw a rectangle
      ctx.beginPath();
      ctx.rect(start.x, start.y, end.x - start.x, end.y - start.y);
      ctx.strokeStyle = "red";
      ctx.stroke();
    }

    start = null;
  };

  const deleteRectangle = (rect: {
    x: number;
    y: number;
    width: number;
    height: number;
  }) => {
    config = {
      ...config,
      rectangles: config.rectangles.filter((r) => r !== rect),
    };

    // Redraw image
    drawBuffer();
    drawRectangles();
  };

  // Cors is disabled on the server, so we need to send the data to the server
  const uploadConfiguration = () => {
    fetch("http://10.0.0.146/api/upload-config", {
      method: "POST",
      mode: "cors",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(config),
    });
  };
</script>

<main class="container mx-auto p-6">
  <div class="flex items-start gap-6">
    <canvas
      bind:this={canvas}
      width="480"
      height="320"
      on:mousedown={onMouseDown}
      on:mouseup={onMouseUp}
    ></canvas>
    <div>
      <h2 class="text-xl">Rectangles</h2>
      <table class="table">
        <thead>
          <tr>
            <th>x</th>
            <th>y</th>
            <th>width</th>
            <th>height</th>
            <th></th>
          </tr>
        </thead>
        <tbody>
          {#each config.rectangles as rect}
            <tr>
              <td>{rect.x}</td>
              <td>{rect.y}</td>
              <td>{rect.width}</td>
              <td>{rect.height}</td>
              <td>
                <button on:click={() => deleteRectangle(rect)} class="btn"
                  >Delete</button
                >
              </td>
            </tr>
          {/each}
        </tbody>
      </table>

      <button on:click={uploadConfiguration} class="btn"
        >Upload configuration</button
      >
      <button on:click={fetchImg} class="btn">Get new image</button>
    </div>
    <div>
      <h2 class="text-xl">Log</h2>
      <pre>{log}</pre>
    </div>
  </div>
</main>
