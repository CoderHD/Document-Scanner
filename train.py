#!/usr/bin/python3
from model import save_model, load_model, eval_loss_on_batches, eval_loss_and_metrics_on_batches, count_params
from data import load_datasets, prepare_pre_dataset, prepare_bm_dataset
import torch
import torch.optim.lr_scheduler as lr_scheduler
from torchinfo import summary
import datetime
from itertools import cycle
from tqdm import tqdm
from benchmark import benchmark_plt_pre, benchmark_plt_bm

warmup_lr = 1e-5
lr = 1e-3
steps_per_epoch = 40
batch_size = 12
device = torch.device("cuda")

mask_epochs = 300
bm_epochs = 300
warmup_epochs = 15
min_learning_rate_before_early_termination = 1e-7
lr_plateau_patience = 3
valid_batch_count = 16
valid_eval_every = 4
lam = 0.85

mask_time_in_hours = 2.0
bm_time_in_hours = 4.0


def cycle(iterable):
    while True:
        for x in iterable:
            yield x
        

def model_to_device(model):
    return model.to(device)


def train_model(model, model_path, epochs, time_in_hours, ds, is_pre, summary_writer):
    optim = torch.optim.Adam(model.parameters(), lr=lr, weight_decay=1e-3)
    warmup_scheduler = lr_scheduler.LinearLR(optim, start_factor=warmup_lr / lr, total_iters=warmup_epochs)
    main_scheduler = lr_scheduler.ReduceLROnPlateau(optim, factor=0.1, patience=lr_plateau_patience)

    start_time = datetime.datetime.now()

    train_ds, valid_ds, test_ds = load_datasets(*ds, batch_size)
    trainds_iter = iter(cycle(train_ds))
    valid_iter = iter(cycle(valid_ds))
    test_iter = iter(test_ds)

    pbar = tqdm(total=epochs)
    pbar.set_description("initializing training and running first epoch")
    
    for epoch in range(epochs):
        train_loss = 0.0

        for _ in range(steps_per_epoch):
            dict = next(trainds_iter)

            for key in dict.keys():
                dict[key] = dict[key].to(device)
                
            x, _ = model.input_and_label_from_dict(dict)

            optim.zero_grad(set_to_none=True)

            if is_pre:
                pred = model(x)
                loss = model.loss(pred, dict)
            else:
                preds = model.forward_all(x)
                
                loss = 0.0

                for i, pred in enumerate(preds):
                    fac = lam ** (len(preds) - 1 - i)
                    loss += fac * model.loss(pred, dict)

                loss /= float(len(preds))
            
            loss.backward()

            optim.step()

            train_loss += loss.detach().cpu().item()

        train_loss /= steps_per_epoch

        if epoch % valid_eval_every == 0:
            valid_loss = eval_loss_on_batches(model, valid_iter, valid_batch_count, device)
            summary_writer.add_scalar("Loss/valid", valid_loss, epoch)

        if epoch < warmup_epochs:
            warmup_scheduler.step()
        else:
            main_scheduler.step(train_loss)


        now = datetime.datetime.now()
        hours_passed = (now - start_time).seconds / (60.0 * 60.0)
        learning_rate = optim.param_groups[-1]['lr']

        summary_writer.add_scalar("Loss/train", train_loss, epoch)
        summary_writer.add_scalar("Learning rate", learning_rate, epoch)

        pbar.update(1)
        pbar.set_description(f"epoch {epoch + 1}/{epochs} completed with {hours_passed:.4f}h passed. training loss: {train_loss:.4f}, validation loss: {valid_loss:.4f}, learning rate: {learning_rate}")

        if hours_passed > time_in_hours:
            pbar.close()
            print(f"hour limit of {time_in_hours:.4f}h passed")
            break

        if learning_rate < min_learning_rate_before_early_termination:
            pbar.close()
            print(f"learning rate {learning_rate} is smaller than {min_learning_rate_before_early_termination}. no further learning progress can be made")
            break

    save_model(model, model_path)

    pbar.close()

    model.set_train(False)

    test_loss, metrics = eval_loss_and_metrics_on_batches(model, test_iter, batch_size, device)

    hparams = {"parameter_count": count_params(model)}

    metric_dict = {"Loss/test": test_loss}
    for (name, item) in metrics:
        metric_dict[name] = item

    summary_writer.add_hparams(hparams, metric_dict)

    if is_pre:
        plt = benchmark_plt_pre(model, ds)
    else:
        plt = benchmark_plt_bm(model, ds)

    summary_writer.add_figure("benchmark", plt)
    
    print(f"test loss: {test_loss:.4f}")


def train_pre_model(model, model_path, summary_writer):
    ds = prepare_pre_dataset()
    train_model(model, model_path, mask_epochs, mask_time_in_hours, ds, True, summary_writer)


def train_bm_model(model, model_path, summary_writer):
    ds = prepare_bm_dataset()
    train_model(model, model_path, bm_epochs, bm_time_in_hours, ds, False, summary_writer)


if __name__ == "__main__":
    import os
    os.environ['TF_CPP_MIN_LOG_LEVEL'] = '3'
    from torch.utils.tensorboard import SummaryWriter
    import seg_model, bm_model

    print("training segmentation model")

    writer = SummaryWriter("runs/main_seg_model")
    model = seg_model.PreModel()
    model = model_to_device(model)
    train_pre_model(model, "models/main_seg_model.pth", summary_writer=writer)
    writer.flush()

    print()
    print("training bm model")

    writer = SummaryWriter("runs/main_bm_model")
    model = bm_model.BMModel(True, False)
    model = model_to_device(model)
    train_bm_model(model, "models/main_bm_model.pth", summary_writer=writer)
    writer.flush()