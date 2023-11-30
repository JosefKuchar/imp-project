<script lang="ts">
  import { onMount } from "svelte";
  import type { MouseEventHandler } from "svelte/elements";

  let canvas: HTMLCanvasElement;
  let canvas2: HTMLCanvasElement;
  let ctx: CanvasRenderingContext2D;
  let ctx2: CanvasRenderingContext2D;
  let start: { x: number; y: number } | null = null;
  let rectangles: { x: number; y: number; width: number; height: number }[] =
    [];
  let buffer: ArrayBuffer;
  let log: string = "";
  let orgRectangleLength = 0;

  onMount(() => {
    ctx = canvas.getContext("2d")!;
    ctx2 = canvas2.getContext("2d")!;
    fetchConfiguration().then(() => fetchImg());
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

  const fetchConfiguration = () =>
    fetch("http://10.0.0.146/config.json")
      .then((res) => res.json())
      .then((c) => {
        rectangles = c["rectangles"];
        orgRectangleLength = rectangles.length;
      });

  const fetchImg = () =>
    fetch("http://10.0.0.146/api/image")
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

  const inferenceImg = () =>
    fetch("http://10.0.0.146/api/inference")
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
    console.log(orgRectangleLength);
    for (let i = 0; i < orgRectangleLength; i++) {
      const img = new ImageData(
        grayScaleToRGBAArray(
          new Uint8ClampedArray(buffer.slice(i * 28 * 28, (i + 1) * 28 * 28))
        ),
        28,
        28
      );
      ctx2.putImageData(img, i * 28, 0);
    }
    const img = new ImageData(
      grayScaleToRGBAArray(
        new Uint8ClampedArray(buffer.slice(28 * 28 * orgRectangleLength))
      ),
      480,
      320
    );
    ctx.putImageData(img, 0, 0);
  };

  const drawRectangles = () => {
    rectangles.forEach((r) => {
      ctx.beginPath();
      ctx.rect(r.x, r.y, r.width, r.height);
      ctx.strokeStyle = "red";
      ctx.stroke();
    });
  };

  const resetLog = () => {
    fetch("http://10.0.0.146/api/log", { method: "DELETE" }).then(() =>
      fetchLog()
    );
  };

  const onMouseDown: MouseEventHandler<HTMLCanvasElement> = (e) => {
    start = { x: e.offsetX, y: e.offsetY };
  };

  const onMouseUp: MouseEventHandler<HTMLCanvasElement> = (e) => {
    const end = { x: e.offsetX, y: e.offsetY };

    if (start) {
      // Add rectangle to list
      rectangles.push({
        x: start.x,
        y: start.y,
        width: end.x - start.x,
        height: end.y - start.y,
      });
      rectangles = rectangles;
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
    rectangles = rectangles.filter((r) => r !== rect);

    // Redraw image
    drawBuffer();
    drawRectangles();
  };

  // Cors is disabled on the server, so we need to send the data to the server
  const uploadConfiguration = () => {
    orgRectangleLength = rectangles.length;
    fetch("http://10.0.0.146/api/upload-config", {
      method: "POST",
      mode: "cors",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ rectangles }),
    });
  };
</script>

<main class="container mx-auto p-6">
  <div class="flex items-start gap-6">
    <div>
      <canvas
        bind:this={canvas}
        width="480"
        height="320"
        on:mousedown={onMouseDown}
        on:mouseup={onMouseUp}
      ></canvas>
      <canvas
        bind:this={canvas2}
        width={28 * rectangles.length}
        height="28"
        class="w-full"
      ></canvas>
    </div>
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
          {#each rectangles as rect}
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
      <button on:click={inferenceImg} class="btn">Inference</button>
      <button on:click={resetLog} class="btn">Reset log</button>
    </div>
    <div>
      <h2 class="text-xl">Log</h2>
      <pre>{log}</pre>
    </div>
  </div>
</main>
