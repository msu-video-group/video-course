import numpy as np


def _measure(predicted_video, reference_video, metric_fn):
    metric_values = [
        metric_fn(frame_pred, frame_true)
        for frame_pred, frame_true in zip(predicted_video, reference_video)
    ]

    return {
        'mean': np.mean(metric_values),
        'std': np.std(metric_values)
    }


def measure_single(fields, reference_video, deinterlace_fn, metrics_dict):
    deinterlaced = deinterlace_fn(fields)
    return {
        label: _measure(deinterlaced, reference_video, metric_fn)
        for label, metric_fn in metrics_dict.items()
    }
