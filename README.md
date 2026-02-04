<!DOCTYPE html>
<html>
<body>

<h1>DCM Agent</h1>

<p>The <strong>DCM Agent</strong> is a daemon responsible for managing device configuration and scheduling tasks such as log uploads and firmware updates. It interacts with the RDK telemetry system and uses RBUS for communication. The agent is designed to be lightweight, efficient, and extensible.</p>

<hr />

<h2>Core Idea</h2>

<p>The DCM Agent is built to:</p>
<ul>
    <li>Parse configuration files and manage device settings.</li>
    <li>Schedule and execute periodic tasks like log uploads and firmware updates.</li>
    <li>Communicate with other components using RBUS (RDK Bus).</li>
    <li>Provide a modular and extensible architecture for future enhancements.</li>
</ul>

<hr />

<h2>Dependencies</h2>

<p>The DCM Agent relies on the following libraries and tools:</p>
<ul>
    <li><strong>RBUS</strong>: For inter-process communication.</li>
    <li><strong>cJSON</strong>: For parsing JSON configuration files.</li>
    <li><strong>pthread</strong>: For multithreading support.</li>
    <li><strong>glibc</strong>: For standard C library functions.</li>
</ul>

<h3>Build Dependencies</h3>
<ul>
    <li><strong>Autotools</strong>: For building the project (<code>autoconf</code>, <code>automake</code>).</li>
    <li><strong>CMake</strong>: For building RBUS.</li>
    <li><strong>Git</strong>: For cloning dependencies.</li>
</ul>

<hr />

<h2>Flow Diagram</h2>

<p>Below is a high-level flow diagram of the DCM Agent's operation:</p>

<pre><code>
@startuml
start
:Initialize DCM Agent;
:Check if Daemon is already running;
if (Daemon running?) then (Yes)
    :Exit;
else (No)
    :Initialize RBUS;
    :Parse Configuration;
    :Start Scheduler;
    :Subscribe to RBUS Events;
    while (Running?)
        :Wait for Events;
        if (Event Received?) then (Yes)
            :Process Event;
            :Schedule Jobs;
        endif
    endwhile
    :Cleanup and Exit;
endif
stop
@enduml
</code></pre>

<hr />

<h2>Sequence Diagram</h2>

<p>Below is a sequence diagram showing the interaction between the DCM Agent and RBUS:</p>

<pre><code>
@startuml
actor User
participant "DCM Agent" as Agent
participant "RBUS" as RBus
participant "Scheduler" as Scheduler

User -> Agent: Start DCM Agent
Agent -> RBus: Initialize RBUS
Agent -> RBus: Subscribe to Events
RBus -> Agent: Event Notification
Agent -> Scheduler: Schedule Job
Scheduler -> Agent: Execute Job
Agent -> RBus: Send Event
@enduml
</code></pre>

<hr />

<h2>API Documentation</h2>

<h3>Scheduler APIs</h3>

<h4><code>dcmSchedInit</code></h4>
<pre><code class="language-c">
INT32 dcmSchedInit();
</code></pre>
<p><strong>Description</strong>: Initializes the scheduler module.</p>
<p><strong>Returns</strong>: <code>0</code> on success, <code>-1</code> on failure.</p>

<h4><code>dcmSchedUnInit</code></h4>
<pre><code class="language-c">
VOID dcmSchedUnInit();
</code></pre>
<p><strong>Description</strong>: Cleans up and deinitializes the scheduler module.</p>

<h4><code>dcmSchedAddJob</code></h4>
<pre><code class="language-c">
VOID* dcmSchedAddJob(INT8 *pJobName, DCMSchedCB pDcmCB, VOID *pUsrData);
</code></pre>
<p><strong>Description</strong>: Adds a new job to the scheduler.</p>
<ul>
    <li><strong>Parameters</strong>:</li>
    <ul>
        <li><code>pJobName</code>: Name of the job.</li>
        <li><code>pDcmCB</code>: Callback function to execute the job.</li>
        <li><code>pUsrData</code>: User data to pass to the callback.</li>
    </ul>
</ul>
<p><strong>Returns</strong>: A handle to the job.</p>

<h4><code>dcmSchedRemoveJob</code></h4>
<pre><code class="language-c">
VOID dcmSchedRemoveJob(VOID *pHandle);
</code></pre>
<p><strong>Description</strong>: Removes a job from the scheduler.</p>
<ul>
    <li><strong>Parameters</strong>:</li>
    <ul>
        <li><code>pHandle</code>: Handle to the job.</li>
    </ul>
</ul>

<h4><code>dcmSchedStartJob</code></h4>
<pre><code class="language-c">
INT32 dcmSchedStartJob(VOID *pHandle, INT8 *pCronPattern);
</code></pre>
<p><strong>Description</strong>: Starts a job with a specified cron pattern.</p>
<ul>
    <li><strong>Parameters</strong>:</li>
    <ul>
        <li><code>pHandle</code>: Handle to the job.</li>
        <li><code>pCronPattern</code>: Cron expression defining the schedule.</li>
    </ul>
</ul>
<p><strong>Returns</strong>: <code>0</code> on success, <code>-1</code> on failure.</p>

<h4><code>dcmSchedStopJob</code></h4>
<pre><code class="language-c">
INT32 dcmSchedStopJob(VOID *pHandle);
</code></pre>
<p><strong>Description</strong>: Stops a running job.</p>
<ul>
    <li><strong>Parameters</strong>:</li>
    <ul>
        <li><code>pHandle</code>: Handle to the job.</li>
    </ul>
</ul>
<p><strong>Returns</strong>: <code>0</code> on success, <code>-1</code> on failure.</p>

<hr />

<h3>RBUS APIs</h3>

<h4><code>dcmRbusInit</code></h4>
<pre><code class="language-c">
INT32 dcmRbusInit(VOID **ppDCMRbusHandle);
</code></pre>
<p><strong>Description</strong>: Initializes the RBUS communication module.</p>
<ul>
    <li><strong>Parameters</strong>:</li>
    <ul>
        <li><code>ppDCMRbusHandle</code>: Pointer to the RBUS handle.</li>
    </ul>
</ul>
<p><strong>Returns</strong>: <code>0</code> on success, <code>-1</code> on failure.</p>

<h4><code>dcmRbusUnInit</code></h4>
<pre><code class="language-c">
VOID dcmRbusUnInit(VOID *pDCMRbusHandle);
</code></pre>
<p><strong>Description</strong>: Cleans up and deinitializes the RBUS module.</p>
<ul>
    <li><strong>Parameters</strong>:</li>
    <ul>
        <li><code>pDCMRbusHandle</code>: RBUS handle.</li>
    </ul>
</ul>

<!-- Continue with the rest of the APIs in the same format -->

</body>
</html>
